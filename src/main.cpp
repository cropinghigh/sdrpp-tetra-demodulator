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
#include <dsp/sink/handler_sink.h>

#include <gui/widgets/constellation_diagram.h>
#include <gui/widgets/file_select.h>

#include "symbol_extractor.h"

#define CONCAT(a, b)    ((std::string(a) + b).c_str())


SDRPP_MOD_INFO {
    /* Name:            */ "tetra_demodulator",
    /* Description:     */ "Tetra demodulator for SDR++(output can be fed to tetra-rx from osmo-tetra)",
    /* Author:          */ "cropinghigh",
    /* Version:         */ 0, 0, 3,
    /* Max instances    */ -1
};

ConfigManager config;

class TetraDemodulatorModule : public ModuleManager::Instance {
public:
    TetraDemodulatorModule(std::string name) {
        this->name = name;

        // Load config
        config.acquire();
        bool created = false;
        if (!config.conf.contains(name)) {
            config.conf[name]["filePath"] = std::string("/tmp/fifo1");
            config.conf[name]["fileOpen"] = false;
            created = true;
        }
        strcpy(filePath, std::string(config.conf[name]["filePath"]).c_str());
        fileOpen = config.conf[name]["fileOpen"];
        config.release(created);

        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 29000, 36000, 29000, 29000, true);

        //Clock recov coeffs
        float recov_bandwidth = 0.09f;
        float recov_dampningFactor = 0.71f;
        float recov_denominator = (1.0f + 2.0*recov_dampningFactor*recov_bandwidth + recov_bandwidth*recov_bandwidth);
        float recov_mu = (4.0f * recov_dampningFactor * recov_bandwidth) / recov_denominator;
        float recov_omega = (4.0f * recov_bandwidth * recov_bandwidth) / recov_denominator;

        mainDemodulator.init(vfo->output, 18000, 36000, 33, 0.35f, 0.1f, 0.01f, recov_omega, recov_mu, 0.005f);
        constDiagSplitter.init(&mainDemodulator.out);
        constDiagSplitter.bindStream(&constDiagStream);
        constDiagSplitter.bindStream(&demodStream);
        constDiagReshaper.init(&constDiagStream, 1024, 0);
        constDiagSink.init(&constDiagReshaper.out, _constDiagSinkHandler, this);
        symbolExtractor.init(&demodStream);
        bitsUnpacker.init(&symbolExtractor.out);
        demodSink.init(&bitsUnpacker.out, _demodSinkHandler, this);

        if(fileOpen) {
            outfile = std::ofstream(filePath, std::ios::binary);
            if(!outfile.is_open()) {
                spdlog::error("Error opening file for tetra_demodulator output!");
            }
        }

        mainDemodulator.start();
        constDiagSplitter.start();
        constDiagReshaper.start();
        constDiagSink.start();
        symbolExtractor.start();
        bitsUnpacker.start();
        demodSink.start();
        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~TetraDemodulatorModule() {
        if(isEnabled()) {
            disable();
        }
        gui::menu.removeEntry(name);
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
        demodSink.start();

        if(outfile.is_open()) {
            outfile.close();
        }
        if(fileOpen) {
            outfile = std::ofstream(filePath, std::ios::binary);
            if(!outfile.is_open()) {
                spdlog::error("Error opening file for tetra_demodulator output!");
            }
        }
        enabled = true;
    }

    void disable() {
        mainDemodulator.stop();
        constDiagSplitter.stop();
        constDiagReshaper.stop();
        constDiagSink.stop();
        symbolExtractor.stop();
        bitsUnpacker.stop();
        demodSink.stop();
        if(outfile.is_open()) {
            outfile.close();
        }
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

        if (_this->fileOpen && _this->enabled) { style::beginDisabled(); }
        if(ImGui::InputText(CONCAT("Out file:##_tetrademod_outfile_", _this->name), _this->filePath, 2047)) {
            config.acquire();
            config.conf[_this->name]["filePath"] = std::string(_this->filePath);
            config.release(true);
        }
        if (_this->fileOpen && _this->enabled) { style::endDisabled(); }
        if (ImGui::Checkbox(CONCAT("Write to file##_tetrademod_writefile_", _this->name), &_this->fileOpen) && _this->enabled) {
            if(_this->fileOpen) {
                if(_this->outfile.is_open()) {
                    _this->outfile.close();
                }
                _this->outfile = std::ofstream(_this->filePath, std::ios::binary);
                if(!_this->outfile.is_open()) {
                    spdlog::error("Error opening file for tetra_demodulator output!");
                }
            } else {
                if(_this->outfile.is_open()) {
                    _this->outfile.close();
                }
            }
            config.conf[_this->name]["fileOpen"] = _this->fileOpen;
        }
        ImGui::Text("Signal constellation: ");
        ImGui::SetNextItemWidth(menuWidth);
        _this->constDiag.draw();
        _this->constDiagMtx.unlock();

        if(!_this->enabled) {
            style::endDisabled();
        }
    }

    static void _constDiagSinkHandler(dsp::complex_t* data, int count, void* ctx) {
        TetraDemodulatorModule* _this = (TetraDemodulatorModule*)ctx;
        _this->constDiagMtx.try_lock();
        dsp::complex_t* cdBuff = _this->constDiag.acquireBuffer();
        if(count == 1024) {
            memcpy(cdBuff, data, count * sizeof(dsp::complex_t));
        }
        _this->constDiag.releaseBuffer();
    }

    static void _demodSinkHandler(uint8_t* data, int count, void* ctx) {
        TetraDemodulatorModule* _this = (TetraDemodulatorModule*)ctx;
        if(_this->outfile.is_open()) {
            _this->outfile.write((char*)data, count * sizeof(uint8_t));
        }
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
    std::mutex constDiagMtx;

    dsp::stream<dsp::complex_t> demodStream;

    dsp::DQPSKSymbolExtractor symbolExtractor;
    //dsp::DifferentialDecoder diffDecoder;
    dsp::BitUnpacker bitsUnpacker;
    dsp::sink::Handler<uint8_t> demodSink;

    char filePath[2048];
    bool fileOpen = false;
    std::ofstream outfile;

};

MOD_EXPORT void _INIT_() {
    std::string root = (std::string)core::args["root"];
    json def = json({});
    config.setPath(root + "/tetra_demodulator_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new TetraDemodulatorModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (TetraDemodulatorModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
