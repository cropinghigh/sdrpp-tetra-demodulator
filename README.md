# sdrpp-tetra-demodulator
Tetra demodulator plugin for SDR++

Designed to provide output to tetra-rx from osmo-tetra-sq5bpf

Building:
  0.  If you have arch-like system, just install package sdrpp-tetra-demodulator-git with all dependencies
  1.  Install SDR++ core headers to /usr/include/sdrpp_core/ (sdrpp-headers-git package for arch-like systems)
  2.  Build:

          mkdir build
          cd build
          cmake ..
          make
          sudo make install

  4.  Enable new module by adding

          "Tetra demodulator": {
            "enabled": true,
            "module": "tetra_demodulator"
          }

      to config.json, or add it via Module manager
      
Usage:
  1.  Find TETRA frequency you want to listen to
  2.  Move VFO to center of it
  3.  If you see circle on first constellation diagram, instead of 4 points, you can temporarily increase Costas loop bandwidth by slider(i usually raise it to around 0.015-0.03)
  4.  When you will see someting around 4 points/lines in every quarter of diagram, you can return Costas loop bandwidth to normal value(~0.001) to increase stability. After that you probably will see four nice points
  5.  To use osmo-tetra-sq5bpf with it, you need:
  
      Build osmo-tetra-sq5bpf itself (you can use my version with fixed compilation issues on latest GCC: https://github.com/cropinghigh/osmo-tetra-sq5bpf)
      
      Create FIFO, if you want live decoding:
      
          mkfifo /tmp/fifo1 (you can change path to anything you want)
        
      Enter path of that FIFO to module
      
      Run tetra-rx:
      
          tetra-rx -s -r -e /tmp/fifo1
          
      Enable writing to file in module
      
      If all done correctly, you will able to see decoded BURSTs from tetra-rx
      
