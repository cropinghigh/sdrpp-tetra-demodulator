#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <module.h>
#include <unistd.h>
#include <fstream>

#include <dsp/demod/psk.h>
#include <dsp/buffer/packer.h>
#include <dsp/routing/splitter.h>
#include <dsp/stream.h>
#include <dsp/convert/mono_to_stereo.h>

#include <gui/widgets/constellation_diagram.h>
#include <gui/widgets/file_select.h>
#include <gui/widgets/volume_meter.h>

#include <utils/flog.h>

#define ENABLE_SYNC_DETECT
#define SYNC_DETECT_BUF 4096
#define SYNC_DETECT_DISPLAY 256

#include "symbol_extractor.h"
#include "gui_widgets.h"
#include "osmotetra_dec.h"

#define CONCAT(a, b)    ((std::string(a) + b).c_str())

#define VFO_SAMPLERATE 36000
#define VFO_BANDWIDTH 30000
#define CLOCK_RECOVERY_BW 0.0628f
#define CLOCK_RECOVERY_DAMPN_F 0.707f
#define CLOCK_RECOVERY_REL_LIM 0.02f
#define RRC_TAP_COUNT 65
#define RRC_ALPHA 0.35f
#define AGC_RATE 0.02f
#define COSTAS_LOOP_BANDWIDTH 0.045f
#define FLL_LOOP_BANDWIDTH 0.005f

SDRPP_MOD_INFO {
    /* Name:            */ "tetra_demodulator",
    /* Description:     */ "Tetra demodulator for SDR++(output can be fed to tetra-rx from osmo-tetra)",
    /* Author:          */ "cropinghigh",
    /* Version:         */ 0, 0, 9,
    /* Max instances    */ -1
};

class TetraDemodulatorModule : public ModuleManager::Instance {
public:
    TetraDemodulatorModule(std::string name) {
        this->name = name;

        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, VFO_BANDWIDTH, VFO_SAMPLERATE, VFO_BANDWIDTH, VFO_BANDWIDTH, true);

        //Clock recov coeffs
        float recov_bandwidth = CLOCK_RECOVERY_BW;
        float recov_dampningFactor = CLOCK_RECOVERY_DAMPN_F;
        float recov_denominator = (1.0f + 2.0*recov_dampningFactor*recov_bandwidth + recov_bandwidth*recov_bandwidth);
        float recov_mu = (4.0f * recov_dampningFactor * recov_bandwidth) / recov_denominator;
        float recov_omega = (4.0f * recov_bandwidth * recov_bandwidth) / recov_denominator;

        mainDemodulator.init(vfo->output, 18000, VFO_SAMPLERATE, RRC_TAP_COUNT, RRC_ALPHA, AGC_RATE, COSTAS_LOOP_BANDWIDTH, FLL_LOOP_BANDWIDTH, recov_omega, recov_mu, CLOCK_RECOVERY_REL_LIM);
        constDiagSplitter.init(&mainDemodulator.out);
        constDiagSplitter.bindStream(&constDiagStream);
        constDiagSplitter.bindStream(&demodStream);
        constDiagReshaper.init(&constDiagStream, 1024, 0);
        constDiagSink.init(&constDiagReshaper.out, _constDiagSinkHandler, this);
        symbolExtractor.init(&demodStream);
        bitsUnpacker.init(&symbolExtractor.out);
        osmotetradecoder.init(&bitsUnpacker.out);
        resamp.init(&osmotetradecoder.out, 8000.0, audioSampleRate);
        outconv.init(&resamp.out);

        // Initialize the sink
        srChangeHandler.ctx = this;
        srChangeHandler.handler = sampleRateChangeHandler;
        stream.init(&outconv.out, &srChangeHandler, audioSampleRate);
        sigpath::sinkManager.registerStream(name, &stream);

        mainDemodulator.start();
        constDiagSplitter.start();
        constDiagReshaper.start();
        constDiagSink.start();
        symbolExtractor.start();
        bitsUnpacker.start();
        osmotetradecoder.start();
        resamp.start();
        outconv.start();
        stream.start();
        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~TetraDemodulatorModule() {
        if(isEnabled()) {
            disable();
        }
        gui::menu.removeEntry(name);
        sigpath::sinkManager.unregisterStream(name);
    }

    void postInit() {}

    void enable() {
        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 29000, 36000, 29000, 29000, true);
        mainDemodulator.setInput(vfo->output);
        mainDemodulator.start();
        constDiagSplitter.start();
        constDiagReshaper.start();
        constDiagSink.start();
        symbolExtractor.start();
        bitsUnpacker.start();
        osmotetradecoder.start();
        resamp.start();
        outconv.start();
        stream.start();

        enabled = true;
    }

    void disable() {
        mainDemodulator.stop();
        constDiagSplitter.stop();
        constDiagReshaper.stop();
        constDiagSink.stop();
        symbolExtractor.stop();
        bitsUnpacker.stop();
        osmotetradecoder.stop();
        resamp.stop();
        outconv.stop();
        stream.stop();
        sigpath::vfoManager.deleteVFO(vfo);
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:

    static void menuHandler(void* ctx) {
        TetraDemodulatorModule* _this = (TetraDemodulatorModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        if(!_this->enabled) {
            style::beginDisabled();
        }

        ImGui::Text("Signal constellation: ");
        ImGui::SetNextItemWidth(menuWidth);
        _this->constDiag.draw();

#ifdef ENABLE_SYNC_DETECT
        float avg = 1.0f - _this->symbolExtractor.stderr;
        ImGui::Text("Signal quality: ");
        ImGui::SameLine();
        ImGui::SigQualityMeter(avg, 0.5f, 1.0f);
        ImGui::BoxIndicator(GImGui->FontSize*2, _this->symbolExtractor.sync ? IM_COL32(5, 230, 5, 255) : IM_COL32(230, 5, 5, 255));
        ImGui::SameLine();
        ImGui::Text(" Sync");
#endif
        int dec_st = _this->osmotetradecoder.getRxState();
        ImGui::BoxIndicator(GImGui->FontSize*2, (dec_st == 0) ? IM_COL32(230, 5, 5, 255) : ((dec_st == 2) ? IM_COL32(5, 230, 5, 255) : IM_COL32(230, 230, 5, 255)));
        ImGui::SameLine();
        ImGui::Text(" Decoder:  %s", (dec_st == 0) ? "Unlocked" : ((dec_st == 2) ? "Locked" : "Know next start"));
        if(dec_st != 2) {
            style::beginDisabled();
        }
        ImGui::Text("Hyperframe: "); ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "%05d", _this->osmotetradecoder.getCurrHyperframe()); ImGui::SameLine();
        ImGui::Text(" | Multiframe: "); ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "%02d", _this->osmotetradecoder.getCurrMultiframe()); ImGui::SameLine();
        ImGui::Text("| Frame: "); ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "%02d", _this->osmotetradecoder.getCurrFrame());
        ImGui::Text("Timeslots: ");
        for(int i = 0; i < 4; i++) {
            switch(_this->osmotetradecoder.getTimeslotContent(i)) {
                case 0:
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.8, 0.8, 0.8, 1.0), "   UL  ");
                    break;
                case 1:
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.95, 0.05, 0.95, 1.0), " DATA  ");
                    break;
                case 2:
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "  NDB  ");
                    break;
                case 3:
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.05, 0.95, 0.95, 1.0), " SYNC  ");
                    break;
                case 4:
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.05, 0.95, 0.05, 1.0), " VOICE ");
                    break;
            }
        }
        int crc_failed = _this->osmotetradecoder.getLastCrcFail();
        if(crc_failed) {
            ImGui::BoxIndicator(GImGui->FontSize*2, IM_COL32(230, 5, 5, 255));
            ImGui::SameLine();
            ImGui::Text(" CRC: "); ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.95, 0.05, 0.05, 1.0), "FAIL");
            style::beginDisabled();
        } else {
            ImGui::BoxIndicator(GImGui->FontSize*2, IM_COL32(5, 230, 5, 255));
            ImGui::SameLine();
            ImGui::Text(" CRC: "); ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.05, 0.95, 0.05, 1.0), "PASS");
        }
        int dl_usg = _this->osmotetradecoder.getDlUsage();
        int ul_usg = _this->osmotetradecoder.getUlUsage();
        ImGui::Text("DL:");ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "%7.3f", ((float)_this->osmotetradecoder.getDlFreq()/1000000.0f));ImGui::SameLine();
        ImGui::Text(" MHz ");ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), (dl_usg == 0 ? "Unalloc" : (dl_usg == 1 ? "Assigned ctl" : (dl_usg == 2 ? "Common ctl" : (dl_usg == 3 ? "Reserved" : "Traffic")))));
        ImGui::Text("UL:");ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "%7.3f", ((float)_this->osmotetradecoder.getUlFreq()/1000000.0f));ImGui::SameLine();
        ImGui::Text(" MHz ");ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), (ul_usg == 0 ? "Unalloc" : "Traffic"));
        ImGui::Text("Access1: ");ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "%c", _this->osmotetradecoder.getAccess1Code());ImGui::SameLine();
        ImGui::Text("/");ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "%d", _this->osmotetradecoder.getAccess1());ImGui::SameLine();
        ImGui::Text("| Access2: ");ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "%c", _this->osmotetradecoder.getAccess2Code());ImGui::SameLine();
        ImGui::Text("/");ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "%d", _this->osmotetradecoder.getAccess2());
        ImGui::Text("MCC: ");ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "%03d", _this->osmotetradecoder.getMcc());ImGui::SameLine();
        ImGui::Text("| MNC: ");ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "%03d", _this->osmotetradecoder.getMnc());ImGui::SameLine();
        ImGui::Text("| CC: ");ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95, 0.95, 0.05, 1.0), "0x%02x", _this->osmotetradecoder.getCc());
        ImVec4 on_color = ImVec4(0.05, 0.95, 0.05, 1.0);
        ImVec4 off_color = ImVec4(0.95, 0.05, 0.05, 1.0);
        ImGui::TextColored(_this->osmotetradecoder.getAdvancedLink() ? on_color : off_color, "Adv. link  ");ImGui::SameLine();
        ImGui::TextColored(_this->osmotetradecoder.getAirEncryption() ? on_color : off_color, "Encryption  ");ImGui::SameLine();
        ImGui::TextColored(_this->osmotetradecoder.getSndcpData() ? on_color : off_color, "SNDCP");
        ImGui::TextColored(_this->osmotetradecoder.getCircuitData() ? on_color : off_color, "Circuit data  ");ImGui::SameLine();
        ImGui::TextColored(_this->osmotetradecoder.getVoiceService() ? on_color : off_color, "Voice  ");ImGui::SameLine();
        ImGui::TextColored(_this->osmotetradecoder.getNormalMode() ? on_color : off_color, "Normal mode");
        ImGui::TextColored(_this->osmotetradecoder.getMigrationSupported() ? on_color : off_color, "Migration  ");ImGui::SameLine();
        ImGui::TextColored(_this->osmotetradecoder.getNeverMinimumMode() ? on_color : off_color, "Never min mode  ");ImGui::SameLine();
        ImGui::TextColored(_this->osmotetradecoder.getPriorityCell() ? on_color : off_color, "Priority cell");
        ImGui::TextColored(_this->osmotetradecoder.getDeregMandatory() ? on_color : off_color, "Dereg req.  ");ImGui::SameLine();
        ImGui::TextColored(_this->osmotetradecoder.getRegMandatory() ? on_color : off_color, "Reg req.");
        if(crc_failed) {
            style::endDisabled();
        }
        if(dec_st != 2) {
            style::endDisabled();
        }
        if(!_this->enabled) {
            style::endDisabled();
        }
    }

    static void _constDiagSinkHandler(dsp::complex_t* data, int count, void* ctx) {
        TetraDemodulatorModule* _this = (TetraDemodulatorModule*)ctx;
        dsp::complex_t* cdBuff = _this->constDiag.acquireBuffer();
        if(count == 1024) {
            memcpy(cdBuff, data, count * sizeof(dsp::complex_t));
        }
        _this->constDiag.releaseBuffer();
    }

    static void sampleRateChangeHandler(float sampleRate, void* ctx) {
        TetraDemodulatorModule* _this = (TetraDemodulatorModule*)ctx;
        _this->audioSampleRate = sampleRate;
        _this->resamp.stop();
        _this->resamp.setOutSamplerate(_this->audioSampleRate);
        _this->resamp.start();
    }

    std::string name;
    bool enabled = true;

    VFOManager::VFO* vfo;

    dsp::PI4DQPSK mainDemodulator;
    dsp::routing::Splitter<dsp::complex_t> constDiagSplitter;
    dsp::stream<dsp::complex_t> constDiagStream;
    dsp::buffer::Reshaper<dsp::complex_t> constDiagReshaper;
    dsp::sink::Handler<dsp::complex_t> constDiagSink;
    ImGui::ConstellationDiagram constDiag;

    dsp::stream<dsp::complex_t> demodStream;

    dsp::DQPSKSymbolExtractor symbolExtractor;
    dsp::BitUnpacker bitsUnpacker;
    dsp::sink::Handler<uint8_t> demodSink;
    dsp::osmotetradec osmotetradecoder;

    EventHandler<float> srChangeHandler;
    dsp::multirate::RationalResampler<float> resamp;
    dsp::convert::MonoToStereo outconv;
    SinkManager::Stream stream;
    double audioSampleRate = 48000.0;

};

MOD_EXPORT void _INIT_() {
    std::string root = (std::string)core::args["root"];
    json def = json({});
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new TetraDemodulatorModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (TetraDemodulatorModule*)instance;
}

MOD_EXPORT void _END_() {

}
