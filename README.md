# sdrpp-tetra-demodulator
Tetra demodulator plugin for SDR++

Designed to provide output to tetra-rx from osmo-tetra-sq5bpf

Signal chain:

VFO->Demodulator(RRC->AGC->Maximum Likelihood(y[n]y'[n]) timing recovery->Costas loop)->Constellation diagram->Symbol extractor->Differential decoder->Bits unpacker->Output UDP sender

Building:

  0.  If you have arch-like system, just install package sdrpp-tetra-demodulator-git with all dependencies

  1.  Install SDR++ core headers to /usr/include/sdrpp_core/, if not installed (sdrpp-headers-git package for arch-like systems)

          git clone https://github.com/AlexandreRouma/SDRPlusPlus.git
          cd "SDRPlusPlus/core/src"
          sudo mkdir -p "/usr/include/sdrpp_core"
          sudo find . -regex ".*\.\(h\|hpp\)" -exec cp --parents \{\} "/usr/include/sdrpp_core" \;

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

  1.  Find TETRA frequency you want to receive

  2.  Move demodulator VFO to the center of it

  3.  After some time, it will sync to the carrier and you'll likely see 4 constellation points(sync requires at least ~10dB of signal)

  4.  To use osmo-tetra-sq5bpf with it, you need:

      Build osmo-tetra-sq5bpf itself (you can use my version with fixed compilation issues for latest GCC: https://github.com/cropinghigh/osmo-tetra-sq5bpf)

      Configure network address and port, or leave them default.

      Start decoder listening on the required port, for example:

          socat - udp-listen:8355 | ./tetra-rx -s -r -e /dev/stdin

      Start network in the module

      If everything was done right, you will see decoded BURSTs from tetra-rx

