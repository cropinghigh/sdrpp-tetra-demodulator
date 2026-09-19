# sdrpp-tetra-demodulator
Tetra demodulator plugin for SDR++

Designed to fully demodulate and decode TETRA downlink signals

Thanks to osmo-tetra authors for their great library

Signal chain:

VFO->Demodulator(AGC->FLL->RRC->Maximum Likelihood(y[n]y'[n]) timing recovery->Costas loop)->Constellation diagram->Symbol extractor->Differential decoder->Bits unpacker->Osmo-tetra decoder->Sink

Binary installing:

Visit the Actions page, find latest commit build artifacts, download tetra_demodulator.so and put it to /usr/lib/sdrpp/plugins/, skipping to the step 4. Don't forget to install libtalloc!

Building:

  0.  If you have arch-like system, just install package sdrpp-tetra-demodulator-git with all dependencies.

      OR 

  1.  Install SDR++ core headers to /usr/include/sdrpp_core/, if not installed. Refer to https://cropinghigh.github.io/sdrpp-moduledb/headerguide.html about how to do that

      OR if you don't want to use my header system, add -DSDRPP_MODULE_CMAKE="/path/to/sdrpp_build_dir/sdrpp_module.cmake" to cmake launch arguments

      Download and patch ETSI TETRA codec(in this repository):

          cd src/decoder/etsi_codec-patches
          ./download_and_patch.sh

      Install libtalloc-dev/talloc via package manager

  2.  Build:

          mkdir build
          cd build
          cmake ..
          make
          sudo make install

  4.  Enable new module by adding it via Module manager

Usage:

  1.  Find TETRA frequency you want to receive

  2.  Move demodulator VFO to the center of it

  3.  After some time, it will sync to the carrier and you'll likely see 4 constellation points(sync requires at least ~20dB of signal)

  4.  If the channel is unencrypted, just wait for the voice activity and listen to it!

Optional encryption/key support
-------------------------------

The OSMO-TETRA decoder can decrypt traffic for which you supply an authorized
80-bit CCK/SCK. This integration supports the implementations included in this
repository: **TEA1, TEA2, and TEA3 only**. TEA4 and the other enum values are
not supported. DCK, MGCK, GCK, authentication bypass, key recovery, brute force,
and key searching are not implemented.

Open **Crypto / Encryption** in the module's OSMO-TETRA view, enter a key-file
path, and select **Load**. **Reload** transactionally replaces a previously
loaded file and **Clear** unloads all keys. The path and key material are not
written to `tetra_demodulator_config.json`. An invalid or missing file reports
an error and leaves the active keystore and clear-TETRA decoder operational.

The text format contains one definition per line; blank lines and lines whose
first non-space character is `#` are ignored:

```
network mcc <decimal> mnc <decimal> ksg_type <1|2|3> security_class <1|2|3>
key mcc <decimal> mnc <decimal> addr <decimal> key_type 1 key_num <decimal> key <20 hex digits>
```

Every key must have a matching network line. `ksg_type` values 1, 2, and 3
select TEA1, TEA2, and TEA3 respectively. `key_type 1` is CCK/SCK; other key
types are rejected because their selection/derivation is not implemented.
Never publish a real network key in logs, screenshots, bug reports, or test
files.

Decryption is attempted only for a resource assignment marked encrypted and
matched to its traffic timeslot/usage marker. The status view reports non-secret
context and explains common unavailable states, including waiting for SYNC or
SYSINFO, an unknown network, or a missing key. The main-carrier number currently
comes from SYSINFO, so decoding away from the main control carrier may not have
enough metadata for safe keystream generation. No values are guessed.

The decoder owns a per-instance keystore and offers a synchronized
`addOrReplaceKey` boundary for authorized external key providers and test
harnesses. Updating the keystore refreshes selection without restarting SDR++;
no external process execution or key-recovery functionality is included.
The inherited osmo-tetra PHY timing state is still process-global; simultaneous
decoder instances therefore retain that pre-existing limitation, although their
key databases and key updates are isolated from one another.

Only receive and decrypt systems or recordings that you are legally authorized
to access. Applicable radio, privacy, and cryptography laws vary by jurisdiction.

 

Running crypto tests without SDR++
----------------------------------

The crypto tests have a standalone CMake project and do not alter the plugin's
normal CMake workflow:

```
cmake -S tests -B build-tests
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```
