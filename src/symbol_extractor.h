#pragma once
#include <dsp/processor.h>

#include <fstream>
#include <iomanip>
#include <sstream>

#include <dsp/loop/phase_control_loop.h>
#include <dsp/taps/windowed_sinc.h>
#include <dsp/multirate/polyphase_bank.h>
#include <dsp/math/step.h>
#include <math.h>

namespace dsp {
    //Symbol mapper + differential decoder
    class DQPSKSymbolExtractor : public Processor<complex_t, uint8_t> {
        using base_type = Processor<complex_t, uint8_t>;
    public:
        DQPSKSymbolExtractor() {}

        DQPSKSymbolExtractor(stream<complex_t>* in) {init(in);}

        inline int process(int count, const complex_t* in, uint8_t* out) {
            for(int i = 0; i < count; i++) {
                complex_t sym_c = in[i];
                bool a = sym_c.im<0;
                bool b = sym_c.re<0;
#ifdef ENABLE_SYNC_DETECT
                complex_t ideal_sym = {b?-0.7071f : 0.7071f, a?-0.7071f : 0.7071f};
                // float dist = fabsf(sqrtf(((ideal_sym.re-sym_c.re)*(ideal_sym.re-sym_c.re))+((ideal_sym.im-sym_c.im)*(ideal_sym.im-sym_c.im))));
                float dist = std::abs(ideal_sym.phase() - sym_c.phase());
                errorbuf[errorptr] = dist;
                errorptr++;
                if(errorptr >= SYNC_DETECT_BUF) {
                    errorptr = 0;
                }
                errordisplayptr++;
                if(errordisplayptr >= SYNC_DETECT_DISPLAY) {
                    float xerr = 0;
                    for(int i = 0; i < SYNC_DETECT_BUF; i++) {
                        xerr+=errorbuf[i];
                    }
                    xerr /= (float)SYNC_DETECT_BUF;
                    stderr = xerr;
                    if(xerr >= 0.35f) {
                        sync = false;
                    } else {
                        sync = true;
                    }
                    errordisplayptr = 0;
                }
#endif
                uint8_t sym = ((a)<<1) | (a!=b); //This mapping is required to make substraction differential decoder work properly
                uint8_t phaseDiff = (sym - prev + 4) % 4;
                switch(phaseDiff) { //Remap phase diffs to actual tetra symbols(swap 0b10 and 0b11)
                    case 0b00:
                        //0
                        out[i] = 0b00;
                        break;
                    case 0b01:
                        //pi/2
                        out[i]=0b01;
                        break;
                    case 0b10:
                        //pi
                        out[i]=0b11;
                        break;
                    case 0b11:
                        //-pi/2
                        out[i]=0b10;
                        break;
                }
                prev = sym;
            }
            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!base_type::out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

#ifdef ENABLE_SYNC_DETECT
        bool sync = false;
        float stderr = 0;
#endif

    private:
        uint8_t prev = 0;
#ifdef ENABLE_SYNC_DETECT
        float errorbuf[SYNC_DETECT_BUF];
        int errorptr = 0;
        int errordisplayptr = 0;
#endif
    };

    //Unpack 2bits/byte to 1, because tetra-rx wants it
    class BitUnpacker : public Processor<uint8_t, uint8_t> {
        using base_type = Processor<uint8_t, uint8_t>;
    public:
        BitUnpacker() {}

        BitUnpacker(stream<uint8_t>* in) { init(in); }

        inline int process(int count, const uint8_t* in, uint8_t* out) {
            for(int i = 0; i < count; i++) {
                out[(i * 2) + 1] = in[i] & 0b01;
                out[(i * 2)] = (in[i] & 0b10) >> 1;
            }
            return count*2;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!base_type::out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

    };

    namespace clock_recovery {
        class COMPLEX_FD : public Processor<complex_t, complex_t> {
            using base_type = Processor<complex_t, complex_t> ;
        public:
            COMPLEX_FD() {}

            COMPLEX_FD(stream<complex_t>* in, double omega, double omegaGain, double muGain, double omegaRelLimit, int interpPhaseCount = 256, int interpTapCount = 256) { init(in, omega, omegaGain, muGain, omegaRelLimit, interpPhaseCount, interpTapCount); }

            ~COMPLEX_FD() {
                if (!base_type::_block_init) { return; }
                base_type::stop();
                dsp::multirate::freePolyphaseBank(interpBank);
                buffer::free(buffer);
            }

            void init(stream<complex_t>* in, double omega, double omegaGain, double muGain, double omegaRelLimit, int interpPhaseCount = 256, int interpTapCount = 256) {
                _omega = omega;
                _omegaGain = omegaGain;
                _muGain = muGain;
                _omegaRelLimit = omegaRelLimit;
                _interpPhaseCount = interpPhaseCount;
                _interpTapCount = interpTapCount;

                pcl.init(_muGain, _omegaGain, 0.0, 0.0, 1.0, _omega, _omega * (1.0 - omegaRelLimit), _omega * (1.0 + omegaRelLimit));
                generateInterpTaps();
                buffer = buffer::alloc<complex_t>(STREAM_BUFFER_SIZE + _interpTapCount);
                bufStart = &buffer[_interpTapCount - 1];

                base_type::init(in);
            }

            void setOmega(double omega) {
                assert(base_type::_block_init);
                std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
                base_type::tempStop();
                _omega = omega;
                offset = 0;
                pcl.phase = 0.0f;
                pcl.freq = _omega;
                pcl.setFreqLimits(_omega * (1.0 - _omegaRelLimit), _omega * (1.0 + _omegaRelLimit));
                base_type::tempStart();
            }

            void setOmegaGain(double omegaGain) {
                assert(base_type::_block_init);
                std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
                _omegaGain = omegaGain;
                pcl.setCoefficients(_muGain, _omegaGain);
            }

            void setMuGain(double muGain) {
                assert(base_type::_block_init);
                std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
                _muGain = muGain;
                pcl.setCoefficients(_muGain, _omegaGain);
            }

            void setOmegaRelLimit(double omegaRelLimit) {
                assert(base_type::_block_init);
                std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
                _omegaRelLimit = omegaRelLimit;
                pcl.setFreqLimits(_omega * (1.0 - _omegaRelLimit), _omega * (1.0 + _omegaRelLimit));
            }

            void setInterpParams(int interpPhaseCount, int interpTapCount) {
                assert(base_type::_block_init);
                std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
                base_type::tempStop();
                _interpPhaseCount = interpPhaseCount;
                _interpTapCount = interpTapCount;
                dsp::multirate::freePolyphaseBank(interpBank);
                buffer::free(buffer);
                generateInterpTaps();
                buffer = buffer::alloc<complex_t>(STREAM_BUFFER_SIZE + _interpTapCount);
                bufStart = &buffer[_interpTapCount - 1];
                base_type::tempStart();
            }

            void reset() {
                assert(base_type::_block_init);
                std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
                base_type::tempStop();
                offset = 0;
                pcl.phase = 0.0f;
                pcl.freq = _omega;
                base_type::tempStart();
            }

            inline int process(int count, const complex_t* in, complex_t* out) {
                // Copy data to work buffer
                memcpy(bufStart, in, count * sizeof(complex_t));

                // Process all samples
                int outCount = 0;
                while (offset < count) {
                    float error;
                    complex_t outVal;
                    complex_t dfdt;

                    // Calculate new output value
                    int phase = std::clamp<int>(floorf(pcl.phase * (float)_interpPhaseCount), 0, _interpPhaseCount - 1);
                    volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&outVal, (lv_32fc_t*)&buffer[offset], interpBank.phases[phase], _interpTapCount);
                    out[outCount++] = outVal;

                    // Calculate derivative of the signal
                    if (phase == 0) {
                        complex_t fT1;
                        volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&fT1, (lv_32fc_t*)&buffer[offset], interpBank.phases[phase+1], _interpTapCount);
                        dfdt = fT1 - outVal;
                    }
                    else if (phase == _interpPhaseCount - 1) {
                        complex_t fT_1;
                        volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&fT_1, (lv_32fc_t*)&buffer[offset], interpBank.phases[phase-1], _interpTapCount);
                        dfdt = outVal - fT_1;
                    }
                    else {
                        complex_t fT_1;
                        complex_t fT1;
                        volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&fT1, (lv_32fc_t*)&buffer[offset], interpBank.phases[phase+1], _interpTapCount);
                        volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&fT_1, (lv_32fc_t*)&buffer[offset], interpBank.phases[phase-1], _interpTapCount);
                        dfdt = (fT1 - fT_1) * 0.5f;
                    }
                    
                    // Calculate error
                    error = ((outVal.re * dfdt.re) + (outVal.im * dfdt.im))*8.0f;

                    // Clamp symbol phase error
                    if (error > 1.0f) { error = 1.0f; }
                    if (error < -1.0f) { error = -1.0f; }

                    // Advance symbol offset and phase
                    pcl.advance(error);
                    float delta = floorf(pcl.phase);
                    offset += delta;
                    pcl.phase -= delta;
                }
                offset -= count;

                // Update delay buffer
                memmove(buffer, &buffer[count], (_interpTapCount - 1) * sizeof(complex_t));

                return outCount;
            }

            int run() {
                int count = base_type::_in->read();
                if (count < 0) { return -1; }

                int outCount = process(count, base_type::_in->readBuf, base_type::out.writeBuf);

                // Swap if some data was generated
                base_type::_in->flush();
                if (outCount) {
                    if (!base_type::out.swap(outCount)) { return -1; }
                }
                return outCount;
            }

            loop::PhaseControlLoop<float, false> pcl;

        protected:
            void generateInterpTaps() {
                double bw = 0.5 / (double)_interpPhaseCount;
                dsp::tap<float> lp = dsp::taps::windowedSinc<float>(_interpPhaseCount * _interpTapCount, dsp::math::hzToRads(bw, 1.0), dsp::window::nuttall, _interpPhaseCount);
                interpBank = dsp::multirate::buildPolyphaseBank<float>(_interpPhaseCount, lp);
                taps::free(lp);
            }

            dsp::multirate::PolyphaseBank<float> interpBank;

            double _omega;
            double _omegaGain;
            double _muGain;
            double _omegaRelLimit;
            int _interpPhaseCount;
            int _interpTapCount;

            int offset = 0;
            complex_t* buffer;
            complex_t* bufStart;
        };
    }

    namespace loop {

        class FLL : public Processor<complex_t, complex_t>  {
            using base_type = Processor<complex_t, complex_t>;
        public:
            FLL() {}

            FLL(stream<complex_t>* in, double bandwidth, int sym_rate, int samp_rate, int filt_size, float filt_a, double initFreq = 0.0, double minFreq = -FL_M_PI, double maxFreq = FL_M_PI) { init(in, bandwidth, sym_rate, samp_rate, filt_size, filt_a, initFreq, minFreq, maxFreq); }

            void init(stream<complex_t>* in, double bandwidth, int sym_rate, int samp_rate, int filt_size, float filt_a, double initFreq = 0.0, double minFreq = -FL_M_PI, double maxFreq = FL_M_PI) {
                _initFreq = initFreq;
                _symbolrate = sym_rate;
                _samplerate = samp_rate;
                _filt_size = filt_size;
                _filt_a = filt_a;

                //Create band-edge and normal filters
                createBandedgeFilters();
                lbandedgerrc.init(NULL, lbandedgerrcTaps);
                hbandedgerrc.init(NULL, hbandedgerrcTaps);

                // Init phase control loop
                float alpha, beta;
                PhaseControlLoop<float>::criticallyDamped(bandwidth, alpha, beta);
                alpha = 0; //we don't need to track phase in FLL
                pcl.init(alpha, beta, 0, -FL_M_PI, FL_M_PI, initFreq, minFreq, maxFreq);
                base_type::init(in);
            }

            //STOLEN FROM GNURADIO!
            void createBandedgeFilters() {
                int sps = _samplerate / _symbolrate;
                const int M = (_filt_size / sps);
                float power = 0;

                // Create the baseband filter by adding two sincs together
                std::vector<float> bb_taps;
                for (int i = 0; i < _filt_size; i++) {
                    float k = -M + i * 2.0f / sps;
                    float tap = math::sinc(_filt_a * k - 0.5f) + math::sinc(_filt_a * k + 0.5f);
                    power += tap;

                    bb_taps.push_back(tap);
                }

                lbandedgerrcTaps = taps::alloc<complex_t>(_filt_size);
                hbandedgerrcTaps = taps::alloc<complex_t>(_filt_size);

                // Create the band edge filters by spinning the baseband
                // filter up and down to the right places in frequency.
                // Also, normalize the power in the filters
                int N = (bb_taps.size() - 1.0f) / 2.0f;
                for (int i = 0; i < _filt_size; i++) {
                    float tap = bb_taps[i] / power;

                    float k = (-N + (int)i) / (2.0f * sps);

                    complex_t t1 = math::phasor(-2.0f * FL_M_PI * (1.0f + _filt_a) * k) * tap;
                    complex_t t2 = math::phasor( 2.0f * FL_M_PI * (1.0f + _filt_a) * k) * tap;

                    lbandedgerrcTaps.taps[_filt_size - i - 1] = t1;
                    hbandedgerrcTaps.taps[_filt_size - i - 1] = t2;
                }
            }

            void setBandwidth(double bandwidth) {
                assert(base_type::_block_init);
                std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
                base_type::tempStop();
                float alpha, beta;
                PhaseControlLoop<float>::criticallyDamped(bandwidth, alpha, beta);
                alpha = 0; //we don't need to track phase in FLL
                pcl.setCoefficients(alpha, beta);
                base_type::tempStart();
            }

            void setInitialFreq(double initFreq) {
                assert(base_type::_block_init);
                std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
                _initFreq = initFreq;
            }

            void setFrequencyLimits(double minFreq, double maxFreq) {
                assert(base_type::_block_init);
                std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
                pcl.setFreqLimits(minFreq, maxFreq);
            }

            void reset() {
                assert(base_type::_block_init);
                std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
                base_type::tempStop();
                pcl.phase = 0;
                pcl.freq = _initFreq;
                base_type::tempStart();
            }

            float dbg_last_err = 0;

            virtual inline int process(int count, complex_t* in, complex_t* out) {
                for (int i = 0; i < count; i++) {
                    complex_t shift = math::phasor(-pcl.phase);
                    complex_t x = in[i] * shift;
                    complex_t lbe_out;
                    complex_t hbe_out;
                    lbandedgerrc.process(1, &(x), &lbe_out);
                    hbandedgerrc.process(1, &(x), &hbe_out);
                    float freqError = hbe_out.fastAmplitude() - lbe_out.fastAmplitude();
                    dbg_last_err = pcl.freq;
                    pcl.advance(freqError);
                    out[i] = x;
                }
                return count;
            }

            int run() {
                int count = base_type::_in->read();
                if (count < 0) { return -1; }

                process(count, base_type::_in->readBuf, base_type::out.writeBuf);

                base_type::_in->flush();
                if (!base_type::out.swap(count)) { return -1; }
                return count;
            }
        protected:
            PhaseControlLoop<float> pcl;
            tap<complex_t> lbandedgerrcTaps;
            filter::FIR<complex_t, complex_t> lbandedgerrc;
            tap<complex_t> hbandedgerrcTaps;
            filter::FIR<complex_t, complex_t> hbandedgerrc;
            float _initFreq;
            int _symbolrate;
            int _samplerate;
            int _filt_size;
            float _filt_a;
            complex_t lastVCO = { 1.0f, 0.0f };
        };

        class PI4DQPSK_COSTAS : public PLL {
            using base_type = PLL;
        public:

            inline int process(int count, complex_t* in, complex_t* out) {
                for (int i = 0; i < count; i++) {
                    complex_t shift = math::phasor(-pcl.phase);
                    complex_t x = in[i] * shift;
                    //perform pi/4 shift on every symbol
                    ph2 += -FL_M_PI/4.0f;
                    if(ph2 >= 2*FL_M_PI) {
                        ph2-=2*FL_M_PI;
                    } else if(ph2 <= -2*FL_M_PI) {
                        ph2+=2*FL_M_PI;
                    }
                    x = x * math::phasor(ph2);
                    pcl.advance(errorFunction(x));
                    out[i] = x;
                }
                return count;
            }

            float ph2 = 0;

        protected:
            inline float errorFunction(complex_t val) {
                float err = 0;
                //default qpsk error function
                err = (math::step(val.re) * val.im) - (math::step(val.im) * val.re);
                return std::clamp<float>(err, -1.0f, 1.0f);
            }

        };
    }

    class PI4DQPSK : public Processor<complex_t, complex_t> {
        using base_type = Processor<complex_t, complex_t>;
    public:
        PI4DQPSK() {}

        PI4DQPSK(stream<complex_t>* in, double symbolrate, double samplerate, int rrcTapCount, double rrcBeta, double agcRate, double costasBandwidth, double fllBandwidth, double omegaGain, double muGain, double omegaRelLimit = 0.01) {
            init(in, symbolrate, samplerate, rrcTapCount, rrcBeta, agcRate, costasBandwidth, fllBandwidth, omegaGain, muGain);
        }

        ~PI4DQPSK() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            taps::free(rrcTaps);
        }

        void init(stream<complex_t>* in, double symbolrate, double samplerate, int rrcTapCount, double rrcBeta, double agcRate, double costasBandwidth, double fllBandwidth, double omegaGain, double muGain, double omegaRelLimit = 0.01) {
            _symbolrate = symbolrate;
            _samplerate = samplerate;
            _rrcTapCount = rrcTapCount;
            _rrcBeta = rrcBeta;

            fll.init(NULL, fllBandwidth, _symbolrate, _samplerate, _rrcTapCount, _rrcBeta, 0, -FL_M_PI/2.0f, FL_M_PI/2.0f);
            rrcTaps = taps::rootRaisedCosine<float>(_rrcTapCount, _rrcBeta, _symbolrate, _samplerate);
            rrc.init(NULL, rrcTaps);
            agc.init(NULL, 1.0, 10e6, agcRate);
            costas.init(NULL, costasBandwidth, 0, 0, -FL_M_PI/10.0f, FL_M_PI/10.0f); //frequency range limit here is REQUIRED!!!
            recov.init(NULL, _samplerate / _symbolrate,  omegaGain, muGain, omegaRelLimit);

            rrc.out.free();
            agc.out.free();
            costas.out.free();
            recov.out.free();

            base_type::init(in);
        }

        void setSymbolrate(double symbolrate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _symbolrate = symbolrate;
            taps::free(rrcTaps);
            rrcTaps = taps::rootRaisedCosine<float>(_rrcTapCount, _rrcBeta, _symbolrate, _samplerate);
            rrc.setTaps(rrcTaps);
            recov.setOmega(_samplerate / _symbolrate);
            base_type::tempStart();
        }

        void setSamplerate(double samplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _samplerate = samplerate;
            taps::free(rrcTaps);
            rrcTaps = taps::rootRaisedCosine<float>(_rrcTapCount, _rrcBeta, _symbolrate, _samplerate);
            rrc.setTaps(rrcTaps);
            recov.setOmega(_samplerate / _symbolrate);
            base_type::tempStart();
        }

        void setRRCParams(int rrcTapCount, double rrcBeta) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _rrcTapCount = rrcTapCount;
            _rrcBeta = rrcBeta;
            taps::free(rrcTaps);
            rrcTaps = taps::rootRaisedCosine<float>(_rrcTapCount, _rrcBeta, _symbolrate, _samplerate);
            rrc.setTaps(rrcTaps);
            base_type::tempStart();
        }

        void setRRCTapCount(int rrcTapCount) {
            setRRCParams(rrcTapCount, _rrcBeta);
        }

        void setRRCBeta(int rrcBeta) {
            setRRCParams(_rrcTapCount, rrcBeta);
        }

        void setAGCRate(double agcRate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            agc.setRate(agcRate);
        }

        void setCostasBandwidth(double bandwidth) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            costas.setBandwidth(bandwidth);
        }

        void setFllBandwidth(double fllBandwidth) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            fll.setBandwidth(fllBandwidth);
        }

        void setMMParams(double omegaGain, double muGain, double omegaRelLimit = 0.01) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            recov.setOmegaGain(omegaGain);
            recov.setMuGain(muGain);
            recov.setOmegaRelLimit(omegaRelLimit);
        }

        void setOmegaGain(double omegaGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            recov.setOmegaGain(omegaGain);
        }

        void setMuGain(double muGain) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            recov.setMuGain(muGain);
        }

        void setOmegaRelLimit(double omegaRelLimit) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            recov.setOmegaRelLimit(omegaRelLimit);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            fll.reset();
            rrc.reset();
            agc.reset();
            costas.reset();
            recov.reset();
            base_type::tempStart();
        }

        inline int process(int count, const complex_t* in, complex_t* out) {
            int ret = count;
            ret = agc.process(ret, (complex_t*) in, out);
            ret = fll.process(ret, out, out);
            ret = rrc.process(ret, out, out);
            ret = recov.process(ret, out, out);
            ret = costas.process(ret, out, out);
            return ret;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!base_type::out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

    protected:
        double _symbolrate;
        double _samplerate;
        int _rrcTapCount;
        double _rrcBeta;

        loop::FLL fll;
        tap<float> rrcTaps;
        filter::FIR<complex_t, float> rrc;
        loop::FastAGC<complex_t> agc;
        loop::PI4DQPSK_COSTAS costas;
        clock_recovery::COMPLEX_FD recov;
    };

}
