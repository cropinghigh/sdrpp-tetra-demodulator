#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <module.h>

#include <dsp/demodulator.h>
#include <dsp/processing.h>
#include <dsp/routing.h>

#include <gui/widgets/constellation_diagram.h>
#include <gui/widgets/file_select.h>

#include "symbol_extractor.h"


#include <unistd.h>

#define CONCAT(a, b)    ((std::string(a) + b).c_str())


SDRPP_MOD_INFO {
    /* Name:            */ "tetra_demodulator",
    /* Description:     */ "Tetra demodulator for SDR++(output can be fed to tetra-rx from osmo-tetra)",
    /* Author:          */ "cropinghigh",
    /* Version:         */ 0, 0, 2,
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
        
        mainDemodulator.init(vfo->output, 36000, 18000, 31, 0.35, 0.4f, debugCostasBw, (0.01*0.01) / 4, 0.01f, 0.005f);
        constDiagSplitter.init(mainDemodulator.out);
        constDiagSplitter.bindStream(&constDiagStream);
        constDiagSplitter.bindStream(&demodStream);
        
        constDiagReshaper.init(&constDiagStream, 1024);
        constDiagSink.init(&constDiagReshaper.out, _constDiagSinkHandler, this);
        
        diffDecoder.init(&demodStream);
        
        constDiag2Splitter.init(&diffDecoder.out);
        constDiag2Splitter.bindStream(&constDiag2Stream);
        constDiag2Splitter.bindStream(&demod2Stream);
        
        constDiag2Reshaper.init(&constDiag2Stream, 1024);
        constDiag2Sink.init(&constDiag2Reshaper.out, _constDiag2SinkHandler, this);
        
        symbolExtractor.init(&demod2Stream);
        
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
        
        diffDecoder.start();
        
        constDiag2Splitter.start();
        constDiag2Reshaper.start();
        constDiag2Sink.start();
        
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
        mainDemodulator.start();
        
        constDiagSplitter.start();
        constDiagReshaper.start();
        constDiagSink.start();
        
        diffDecoder.start();
        
        constDiag2Splitter.start();
        constDiag2Reshaper.start();
        constDiag2Sink.start();
        
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
        
        demodSink.start();
        enabled = true;
    }

    void disable() {
        mainDemodulator.stop();
        
        constDiagSplitter.stop();
        constDiagReshaper.stop();
        constDiagSink.stop();
        
        diffDecoder.stop();
        
        constDiag2Splitter.stop();
        constDiag2Reshaper.stop();
        constDiag2Sink.stop();
        
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

        if (_this->enabled) {
            if(ImGui::SliderFloat(CONCAT("Costas loop bandwidth##_tetrademod_costasbw_", _this->name), &_this->debugCostasBw, 0.0f, 0.05f, "%.6f")) {
                _this->mainDemodulator.setCostasLoopBw(_this->debugCostasBw); //NOT BUG, BUT A FEATURE!(i currently don't know how to automatically reduce costas loop bandwidth, when it synchronizes to carrier)
            }
            if (_this->fileOpen && _this->enabled) { style::beginDisabled(); }
            if(ImGui::InputText(CONCAT("Output file path:##_tetrademod_outfile_", _this->name), _this->filePath, 2047)) {
                config.acquire();
                config.conf[_this->name]["filePath"] = std::string(_this->filePath);
                config.release(true);
            }
            if (_this->fileOpen && _this->enabled) { style::endDisabled(); }
            if (ImGui::Checkbox(CONCAT("Write to file##_tetrademod_writefile_", _this->name), &_this->fileOpen)) {
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
            ImGui::Text("Demodulated: ");
            ImGui::SetNextItemWidth(menuWidth);
            _this->constDiag.draw();
            
            ImGui::Text("Diff decoded: ");
            ImGui::SetNextItemWidth(menuWidth);
            _this->constDiag2.draw();
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
    
    static void _constDiag2SinkHandler(dsp::complex_t* data, int count, void* ctx) {
        TetraDemodulatorModule* _this = (TetraDemodulatorModule*)ctx;
        
        dsp::complex_t* cdBuff = _this->constDiag2.acquireBuffer();
        if(count == 1024) {
            memcpy(cdBuff, data, count * sizeof(dsp::complex_t));
        }
        _this->constDiag2.releaseBuffer();
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
    
    dsp::PSKDemod<4, false> mainDemodulator;
    
    float debugCostasBw = 0.001f;
    dsp::Splitter<dsp::complex_t> constDiagSplitter;
    dsp::stream<dsp::complex_t> constDiagStream;
    dsp::Packer<dsp::complex_t> constDiagReshaper;
    dsp::HandlerSink<dsp::complex_t> constDiagSink;
    ImGui::ConstellationDiagram constDiag;
    
    dsp::stream<dsp::complex_t> demodStream;
    
    dsp::Splitter<dsp::complex_t> constDiag2Splitter;
    dsp::stream<dsp::complex_t> constDiag2Stream;
    dsp::Packer<dsp::complex_t> constDiag2Reshaper;
    dsp::HandlerSink<dsp::complex_t> constDiag2Sink;
    ImGui::ConstellationDiagram constDiag2;
    
    dsp::stream<dsp::complex_t> demod2Stream;
    
    dsp::DQPSKSymbolExtractor symbolExtractor;
    dsp::DifferentialDecoder diffDecoder;
    dsp::BitUnpacker bitsUnpacker;
    dsp::HandlerSink<uint8_t> demodSink;
    
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
