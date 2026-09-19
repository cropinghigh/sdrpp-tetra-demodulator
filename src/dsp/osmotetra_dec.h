#pragma once

#include <dsp/processor.h>
#include <array>
#include <algorithm>
#include <mutex>
#include <string>

// #include <osmocom/core/utils.h>
// #include <osmocom/core/talloc.h>

extern "C" {
    #include "tetra_common.h"
    #include "tetra_upper_mac.h"
    #include "crypto/tetra_crypto.h"
    #include <phy/tetra_burst.h>
    #include <phy/tetra_burst_sync.h>
    #include "c-code/channel.h"
    #include "c-code/source.h"
}

namespace dsp {

    struct TetraCryptoStatus {
        uint32_t networkCount = 0;
        uint32_t keyCount = 0;
        int mcc = -1, mnc = -1, colourCode = -1, locationArea = -1;
        int carrierNumber = -1, hyperframe = -1, cckId = -1;
        int ksg = 0, securityClass = 0, timeslot = 0, usageMarker = 0;
        int encryptionMode = 0;
        bool networkSelected = false, keySelected = false, trafficEncrypted = false;
        bool decryptAttempted = false, decryptSucceeded = false;
        std::string state;
    };

    class osmotetradec : public Processor<uint8_t, float> {
        using base_type = Processor<uint8_t, float>;
    public:
        osmotetradec() {}
        
        ~osmotetradec() {
            tetra_crypto_db_clear(&crypto_db);
            free(tms->fragslots);
            free(trs);
            free(tms ? tms->t_display_st : nullptr);
            free(tms ? tms->tcs : nullptr);
            free(tms);
            
            free(conv_data);
            // talloc_free(trs);
            // talloc_free(tms->t_display_st);
            // talloc_free(tms->tcs);
            // talloc_free(tms);
        }

        osmotetradec(stream<uint8_t>* in) { init(in); }
        
        void init(stream<uint8_t>* in)  {
            /* Initialize tetra mac state and crypto state */
// #ifdef OTC_GLOBAL
//             tms = talloc_zero(OTC_GLOBAL, struct tetra_mac_state);
// #else
//             tms = talloc_zero(tetra_tall_ctx, struct tetra_mac_state);
// #endif
//             tetra_mac_state_init(tms);
//             tms->tcs = talloc_zero(NULL, struct tetra_crypto_state);
//             tms->t_display_st = talloc_zero(NULL, struct tetra_display_state);
//             tetra_crypto_state_init(tms->tcs);
// 
// #ifdef OTC_GLOBAL
//             trs = talloc_zero(OTC_GLOBAL, struct tetra_rx_state);
// #else
//             trs = talloc_zero(tetra_tall_ctx, struct tetra_rx_state);
// #endif
            tms = (struct tetra_mac_state*)malloc(sizeof(struct tetra_mac_state));
            memset(tms, 0, sizeof(struct tetra_mac_state));
            tetra_mac_state_init(tms);
            tms->tcs = (struct tetra_crypto_state*)malloc(sizeof(struct tetra_crypto_state));
            memset(tms->tcs, 0, sizeof(struct tetra_crypto_state));
            tms->t_display_st = (struct tetra_display_state*)malloc(sizeof(struct tetra_display_state));
            memset(tms->t_display_st, 0, sizeof(struct tetra_display_state));
            tetra_crypto_state_init(tms->tcs);
            tetra_crypto_db_init(&crypto_db);
            tms->tcs->db = &crypto_db;
            trs = (struct tetra_rx_state*)malloc(sizeof(struct tetra_rx_state));
            memset(trs, 0, sizeof(struct tetra_rx_state));
            tms->fragslots = (struct fragslot*)malloc(sizeof(struct fragslot)*FRAGSLOT_NR_SLOTS);
            memset(tms->fragslots, 0, sizeof(struct fragslot)*FRAGSLOT_NR_SLOTS);

            conv_data = (float*)malloc(sizeof(float)*STREAM_BUFFER_SIZE);
            memset(conv_data, 0, sizeof(float)*STREAM_BUFFER_SIZE);


            trs->burst_cb_priv = tms;

            tms->put_voice_data = put_voice_data;
            tms->put_voice_data_ctx = this;
            tms->last_frame = 0;
            tms->curr_active_timeslot = 0;

            Init_Decod_Tetra();

            out_tmp_buff.init(32768);

            base_type::init(in);
        }

        //return current RX state. 0=unlocked, 1=know_next_start, 2=locked
        int getRxState() {
            switch(trs->state) {
                case RX_S_LOCKED:
                    return 2;
                case RX_S_KNOW_FSTART:
                    return 1;
                default:
                    return 0;
            }
        }

        int getCurrHyperframe() {
            return tms->t_display_st->curr_hyperframe;
        }
        int getCurrMultiframe() {
            return tms->t_display_st->curr_multiframe;
        }
        int getCurrFrame() {
            return tms->t_display_st->curr_frame;
        }
        int getTimeslotContent(int ts) { //0-other, 1-NORM1, 2-NORM2, 3-SYNC, 4-VOICE
            return tms->t_display_st->timeslot_content[ts];
        }
        int getDlUsage() {
            return tms->t_display_st->dl_usage;
        }
        int getUlUsage() {
            return tms->t_display_st->ul_usage;
        }
        char getAccess1Code() {
            return tms->t_display_st->access1_code;
        }
        char getAccess2Code() {
            return tms->t_display_st->access2_code;
        }
        int getAccess1() {
            return tms->t_display_st->access1;
        }
        int getAccess2() {
            return tms->t_display_st->access2;
        }
        int getDlFreq() {
            return tms->t_display_st->dl_freq;
        }
        int getUlFreq() {
            return tms->t_display_st->ul_freq;
        }
        int getMcc() {
            return tms->t_display_st->mcc;
        }
        int getMnc() {
            return tms->t_display_st->mnc;
        }
        int getCc() {
            return tms->t_display_st->cc;
        }
        bool getLastCrcFail() {
            return tms->t_display_st->last_crc_fail;
        }
        bool getAdvancedLink() {
            return tms->t_display_st->advanced_link;
        }
        bool getAirEncryption() {
            return tms->t_display_st->air_encryption;
        }
        bool getSndcpData() {
            return tms->t_display_st->sndcp_data;
        }
        bool getCircuitData() {
            return tms->t_display_st->circuit_data;
        }
        bool getVoiceService() {
            return tms->t_display_st->voice_service;
        }
        bool getNormalMode() {
            return tms->t_display_st->normal_mode;
        }
        bool getMigrationSupported() {
            return tms->t_display_st->migration_supported;
        }
        bool getNeverMinimumMode() {
            return tms->t_display_st->never_minimum_mode;
        }
        bool getPriorityCell() {
            return tms->t_display_st->priority_cell;
        }
        bool getDeregMandatory() {
            return tms->t_display_st->dereg_mandatory;
        }
        bool getRegMandatory() {
            return tms->t_display_st->reg_mandatory;
        }

        bool loadKeyStore(const std::string& path, std::string& error) {
            std::lock_guard<std::mutex> guard(crypto_mutex);
            char detail[256] = {};
            if (tetra_crypto_db_load(&crypto_db, path.c_str(), detail, sizeof(detail))) {
                error = detail;
                return false;
            }
            key_store_path = path;
            resetCryptoReferences();
            error.clear();
            return true;
        }

        bool reloadKeyStore(std::string& error) {
            std::string path;
            { std::lock_guard<std::mutex> guard(crypto_mutex); path = key_store_path; }
            if (path.empty()) { error = "no key file has been loaded"; return false; }
            return loadKeyStore(path, error);
        }

        void clearKeyStore() {
            std::lock_guard<std::mutex> guard(crypto_mutex);
            tetra_crypto_db_clear(&crypto_db);
            key_store_path.clear();
            resetCryptoReferences();
        }

        bool addOrReplaceKey(uint32_t mcc, uint32_t mnc, int ksg, int securityClass,
                             uint32_t keyNumber, const std::array<uint8_t, 10>& material,
                             std::string& error) {
            std::lock_guard<std::mutex> guard(crypto_mutex);
            tetra_netinfo network = {mcc, mnc, (tetra_ksg_type)ksg,
                                     (tetra_security_class)securityClass};
            tetra_key key = {};
            key.mcc = mcc; key.mnc = mnc; key.key_type = KEYTYPE_CCK_SCK;
            key.key_num = keyNumber;
            std::copy(material.begin(), material.end(), key.key);
            char detail[256] = {};
            if (tetra_crypto_db_add_or_replace(&crypto_db, &network, &key,
                                                detail, sizeof(detail))) {
                error = detail;
                return false;
            }
            resetCryptoReferences();
            error.clear();
            return true;
        }

        TetraCryptoStatus getCryptoStatus() const {
            std::lock_guard<std::mutex> guard(crypto_mutex);
            TetraCryptoStatus result;
            result.networkCount = crypto_db.num_nets; result.keyCount = crypto_db.num_keys;
            if (!tms || !tms->tcs) { result.state = "decoder not initialized"; return result; }
            const tetra_crypto_state *state = tms->tcs;
            result.mcc = state->mcc == UINT32_MAX ? -1 : (int)state->mcc;
            result.mnc = state->mnc == UINT32_MAX ? -1 : (int)state->mnc;
            result.colourCode = state->cc; result.locationArea = state->la;
            result.carrierNumber = state->cn; result.hyperframe = state->hn;
            result.cckId = state->cck_id == UINT32_MAX ? -1 : (int)state->cck_id;
            result.networkSelected = state->network; result.keySelected = state->cck;
            if (state->network) { result.ksg = state->network->ksg_type; result.securityClass = state->network->security_class; }
            int tn = t_phy_state.time.tn;
            result.timeslot = tn;
            if (tn >= 1 && tn <= 4) {
                const auto& traffic = tms->traffic_crypto[tn - 1];
                result.usageMarker = traffic.usage_marker; result.encryptionMode = traffic.encryption_mode;
                result.trafficEncrypted = traffic.assigned && traffic.encrypted;
                result.decryptAttempted = traffic.decrypt_attempted; result.decryptSucceeded = traffic.decrypt_succeeded;
            }
            if (!crypto_db.num_keys) result.state = "no keys loaded";
            else if (state->mcc == UINT32_MAX || state->mnc == UINT32_MAX) result.state = "waiting for SYNC";
            else if (state->hn < 0 || state->cn < 0 || state->la < 0 || state->cc < 0 || state->cck_id == UINT32_MAX) result.state = "waiting for SYSINFO";
            else if (!state->network) result.state = "network not in keystore";
            else if (!state->cck) result.state = "key not found";
            else if (result.trafficEncrypted && result.decryptAttempted && !result.decryptSucceeded) result.state = "decrypt failed";
            else result.state = "ready";
            return result;
        }

        inline int process(int count, const uint8_t* in, float* out)  {
            std::lock_guard<std::mutex> guard(crypto_mutex);
            int outcnt = 0;
            /* tetra_burst_sync_in()/make_bitbuf_space() corrupt the trs struct
             * (overflowing bitbuf[4096] into burst_cb_priv) when fed more bits
             * than the bitbuf can hold, so feed it in safe-sized chunks. */
            const int CHUNK = 2048;
            for (int off = 0; off < count; off += CHUNK) {
                tetra_burst_sync_in(trs, (uint8_t*)in + off, std::min(CHUNK, count - off));
            }
            if(out_tmp_buff.getReadable(false) > 0) {
                outcnt += out_tmp_buff.read(out, out_tmp_buff.getReadable(false));
            }
            outSymsCtr += outcnt;
            inSymsCtr += count;
            int requiredOut = inSymsCtr * 8 / 36;
            int remainingOut = requiredOut - outSymsCtr;
            bool decoding = (tms->t_display_st->timeslot_content[0] == 4) | (tms->t_display_st->timeslot_content[1] == 4) | (tms->t_display_st->timeslot_content[2] == 4) | (tms->t_display_st->timeslot_content[3] == 4);
            if(remainingOut > 0 && !decoding) {
                memset(&(out[outcnt]), 0, remainingOut*sizeof(float));
                outcnt += remainingOut;
            }
            outSymsCtr -= (std::min(outSymsCtr, requiredOut));
            inSymsCtr -= requiredOut * 36 / 8;
            return outcnt;
        }

        int run()  {
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

        static void put_voice_data(void* ctx, int count, int16_t* data) {
            osmotetradec* _this = (osmotetradec*) ctx;

            volk_16i_s32f_convert_32f(_this->conv_data, data, 32768.0f, count);
            if(_this->out_tmp_buff.getWritable(false) >= count) {
                _this->out_tmp_buff.write(_this->conv_data, count);
            }
        }

    private:
        void resetCryptoReferences() {
            if (!tms || !tms->tcs) return;
            for (int i = 0; i < FRAGSLOT_NR_SLOTS; i++) cleanup_fragslot(&tms->fragslots[i]);
            memset(tms->traffic_crypto, 0, sizeof(tms->traffic_crypto));
            tetra_crypto_refresh(tms->tcs);
        }
        int inSymsCtr = 0;
        int outSymsCtr = 0;
        void *tetra_tall_ctx = NULL;
        struct tetra_rx_state *trs = NULL;
        struct tetra_mac_state *tms = NULL;
        struct tetra_crypto_database crypto_db = {};
        mutable std::mutex crypto_mutex;
        std::string key_store_path;
        float *conv_data = NULL;
        buffer::RingBuffer<float> out_tmp_buff;
    };

}
