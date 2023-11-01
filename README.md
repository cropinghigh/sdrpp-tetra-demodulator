# sdrpp-tetra-demodulator
Tetra demodulator plugin for SDR++

Designed to fully demodulate and decode TETRA downlink signals

Thanks to osmo-tetra authors for their great library

Signal chain:

VFO->Demodulator(AGC->FLL->RRC->Maximum Likelihood(y[n]y'[n]) timing recovery->Costas loop)->Constellation diagram->Symbol extractor->Differential decoder->Bits unpacker->Osmo-tetra decoder->Sink

Building:

  0.  If you have arch-like system, just install package sdrpp-tetra-demodulator-git with all dependencies

  1.  Install SDR++ core headers to /usr/include/sdrpp_core/, if not installed (sdrpp-headers-git package for arch-like systems)

          git clone https://github.com/AlexandreRouma/SDRPlusPlus.git
          cd "SDRPlusPlus/core/src"
          sudo mkdir -p "/usr/include/sdrpp_core"
          sudo find . -regex ".*\.\(h\|hpp\)" -exec cp --parents \{\} "/usr/include/sdrpp_core" \;

      Download and patch ETSI TETRA codec(in this repository):

          cd src/decoder/etsi_codec-patches
          ./download_and_patch.sh

      Install libosmocore via package manager

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

  3.  After some time, it will sync to the carrier and you'll likely see 4 constellation points(sync requires at least ~20dB of signal)

  4.  If the channel is unencrypted, just wait for the voice activity and listen to it!

