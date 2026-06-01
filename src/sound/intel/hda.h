/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef KOBALT_HDA_H
#define KOBALT_HDA_H

#ifdef __KERNEL__
#  include <kernel.h>
#  include <pci.h>
#else
#  include <stdint.h>
#  include <stddef.h>
#  include <stdbool.h>
#endif

#define HDA_MAX_CODECS          4u
#define HDA_MAX_WIDGETS         256u
#define HDA_MAX_CONNS           32u
#define HDA_MAX_STREAMS         8u
#define HDA_MAX_OUTPUT_STREAMS  4u
#define HDA_MAX_INPUT_STREAMS   2u
#define HDA_MAX_BIDI_STREAMS    2u
#define HDA_BDL_ENTRIES         32u
#define HDA_PERIOD_BYTES        4096u
#define HDA_CORB_ENTRIES        256u
#define HDA_RIRB_ENTRIES        256u
#define HDA_MIXER_CHANNELS      8u
#define HDA_MAX_AUDIO_PATHS     8u
#define HDA_MAX_EQ_BANDS        10u
#define HDA_VERB_TIMEOUT_US     1000u

#define HDA_PCI_CLASS           0x04u
#define HDA_PCI_SUBCLASS        0x03u
#define HDA_PCI_VENDOR_INTEL    0x8086u
#define HDA_PCI_VENDOR_AMD      0x1022u
#define HDA_PCI_VENDOR_NVIDIA   0x10DEu

#define HDA_DEV_ICH6            0x2668u
#define HDA_DEV_ICH7            0x27D8u
#define HDA_DEV_ICH8            0x284Bu
#define HDA_DEV_ICH9            0x293Eu
#define HDA_DEV_ICH10           0x3A3Eu
#define HDA_DEV_PCH_IBX         0x3B56u
#define HDA_DEV_PCH_CPT         0x1C20u
#define HDA_DEV_PCH_PPT         0x1E20u
#define HDA_DEV_PCH_LPT         0x8C20u
#define HDA_DEV_PCH_WPT         0x8CA0u
#define HDA_DEV_PCH_SPT         0xA170u
#define HDA_DEV_PCH_KBL         0xA171u
#define HDA_DEV_PCH_CNL         0xA348u
#define HDA_DEV_PCH_CML         0x02C8u
#define HDA_DEV_PCH_TGL         0xA0C8u
#define HDA_DEV_PCH_ADL         0x7AD0u

#define HDA_REG_GCAP            0x00u
#define HDA_REG_VMIN            0x02u
#define HDA_REG_VMAJ            0x03u
#define HDA_REG_OUTPAY          0x04u
#define HDA_REG_INPAY           0x06u
#define HDA_REG_GCTL            0x08u
#define HDA_REG_WAKEEN          0x0Cu
#define HDA_REG_STATESTS        0x0Eu
#define HDA_REG_GSTS            0x10u
#define HDA_REG_INTCTL          0x20u
#define HDA_REG_INTSTS          0x24u
#define HDA_REG_WALCLK          0x30u
#define HDA_REG_SSYNC           0x38u
#define HDA_REG_CORBLBASE       0x40u
#define HDA_REG_CORBUBASE       0x44u
#define HDA_REG_CORBWP          0x48u
#define HDA_REG_CORBRP          0x4Au
#define HDA_REG_CORBCTL         0x4Cu
#define HDA_REG_CORBSTS         0x4Du
#define HDA_REG_CORBSIZE        0x4Eu
#define HDA_REG_RIRBLBASE       0x50u
#define HDA_REG_RIRBUBASE       0x54u
#define HDA_REG_RIRBWP          0x58u
#define HDA_REG_RINTCNT         0x5Au
#define HDA_REG_RIRBCTL         0x5Cu
#define HDA_REG_RIRBSTS         0x5Du
#define HDA_REG_RIRBSIZE        0x5Eu
#define HDA_REG_IC              0x60u
#define HDA_REG_IR              0x64u
#define HDA_REG_IRS             0x68u
#define HDA_REG_DPIBLBASE       0x70u
#define HDA_REG_DPIBUBASE       0x74u
#define HDA_REG_SD_BASE         0x80u
#define HDA_REG_SD_SIZE         0x20u

#define HDA_SD_CTL              0x00u
#define HDA_SD_STS              0x03u
#define HDA_SD_LPIB             0x04u
#define HDA_SD_CBL              0x08u
#define HDA_SD_LVI              0x0Cu
#define HDA_SD_FIFOW            0x0Eu
#define HDA_SD_FMT              0x10u
#define HDA_SD_BDPL             0x18u
#define HDA_SD_BDPU             0x1Cu

#define HDA_GCAP_64OK           (1u << 0)
#define HDA_GCAP_NSDO_SHIFT     11u
#define HDA_GCAP_NSDO_MASK      (0x3u << 11)
#define HDA_GCAP_BSS_SHIFT      3u
#define HDA_GCAP_BSS_MASK       (0x1Fu << 3)
#define HDA_GCAP_ISS_SHIFT      8u
#define HDA_GCAP_ISS_MASK       (0xFu << 8)
#define HDA_GCAP_OSS_SHIFT      12u
#define HDA_GCAP_OSS_MASK       (0xFu << 12)

#define HDA_GCTL_RESET          (1u << 0)
#define HDA_GCTL_FCNTRL         (1u << 1)
#define HDA_GCTL_UNSOL          (1u << 8)

#define HDA_INTCTL_GIE          (1u << 31)
#define HDA_INTCTL_CIE          (1u << 30)
#define HDA_INTCTL_SIE_ALL      0x3FFFFFFFu

#define HDA_INTSTS_GIS          (1u << 31)
#define HDA_INTSTS_CIS          (1u << 30)

#define HDA_CORBCTL_MEIE        (1u << 0)
#define HDA_CORBCTL_RUN         (1u << 1)

#define HDA_RINGBUF_SIZE_2      0x00u
#define HDA_RINGBUF_SIZE_16     0x01u
#define HDA_RINGBUF_SIZE_256    0x02u

#define HDA_CORBRP_RST          (1u << 15)

#define HDA_RIRBCTL_RINTCTL     (1u << 0)
#define HDA_RIRBCTL_DMAEN       (1u << 1)
#define HDA_RIRBCTL_OIC         (1u << 2)

#define HDA_RIRBWP_RST          (1u << 15)

#define HDA_IRS_ICB             (1u << 0)
#define HDA_IRS_IRV             (1u << 1)

#define HDA_SDCTL_SRST          (1u << 0)
#define HDA_SDCTL_RUN           (1u << 1)
#define HDA_SDCTL_IOCE          (1u << 2)
#define HDA_SDCTL_FEIE          (1u << 3)
#define HDA_SDCTL_DEIE          (1u << 4)
#define HDA_SDCTL_STRIPE_MASK   (0x3u << 16)
#define HDA_SDCTL_TP            (1u << 18)
#define HDA_SDCTL_DIR           (1u << 19)
#define HDA_SDCTL_STRM_SHIFT    20u
#define HDA_SDCTL_STRM_MASK     (0xFu << 20)

#define HDA_SDSTS_BCIS          (1u << 2)
#define HDA_SDSTS_FIFOE         (1u << 3)
#define HDA_SDSTS_DESE          (1u << 4)
#define HDA_SDSTS_FIFORDY       (1u << 5)

#define HDA_BDL_IOC             (1u << 0)

#define HDA_FMT_TYPE_PCM        (0u << 15)
#define HDA_FMT_TYPE_FLOAT      (1u << 15)
#define HDA_FMT_TYPE_AC3        (1u << 14)
#define HDA_FMT_BASE_48         (0u << 14)
#define HDA_FMT_BASE_441        (1u << 14)
#define HDA_FMT_MULT_1X         (0u << 11)
#define HDA_FMT_MULT_2X         (1u << 11)
#define HDA_FMT_MULT_3X         (2u << 11)
#define HDA_FMT_MULT_4X         (3u << 11)
#define HDA_FMT_DIV(n)          (((n)-1u) << 8)
#define HDA_FMT_BITS_8          (0u << 4)
#define HDA_FMT_BITS_16         (1u << 4)
#define HDA_FMT_BITS_20         (2u << 4)
#define HDA_FMT_BITS_24         (3u << 4)
#define HDA_FMT_BITS_32         (4u << 4)
#define HDA_FMT_CHAN(n)         ((n) - 1u)

#define HDA_VERB(codec, nid, verb, payload) \
    (((uint32_t)(codec)   << 28) | \
     ((uint32_t)(nid)     << 20) | \
     ((uint32_t)(verb)    <<  8) | \
     ((uint32_t)(payload) &  0xFFu))

#define HDA_VERB4(codec, nid, verb4, payload16) \
    (((uint32_t)(codec)    << 28) | \
     ((uint32_t)(nid)      << 20) | \
     ((uint32_t)(verb4)    << 16) | \
     ((uint32_t)(payload16) & 0xFFFFu))

#define HDA_VERB_GET_PARAM(codec, nid, param) \
    HDA_VERB(codec, nid, 0xF00u, param)

#define HDA_VERB_GET_STREAM_FMT     0xA00u
#define HDA_VERB_GET_AMP_GAIN       0xB00u
#define HDA_VERB_GET_PROC_COEFF     0xC00u
#define HDA_VERB_GET_COEFF_IDX      0xD00u
#define HDA_VERB_GET_CONN_SEL       0xF01u
#define HDA_VERB_GET_CONN_LIST      0xF02u
#define HDA_VERB_GET_PROC_STATE     0xF03u
#define HDA_VERB_GET_SDI_SELECT     0xF04u
#define HDA_VERB_GET_POWER_STATE    0xF05u
#define HDA_VERB_GET_CONV_FMT       0xF06u
#define HDA_VERB_GET_SPDIF_CTL      0xF0Du
#define HDA_VERB_GET_SPDIF_STATUS   0xF0Eu
#define HDA_VERB_GET_PIN_CTRL       0xF07u
#define HDA_VERB_GET_UNSOL_RESP     0xF08u
#define HDA_VERB_GET_PIN_SENSE      0xF09u
#define HDA_VERB_GET_EAPD_BTL       0xF0Cu
#define HDA_VERB_GET_GPI_DATA       0xF10u
#define HDA_VERB_GET_GPI_WAKEEN     0xF11u
#define HDA_VERB_GET_GPI_STICKY     0xF12u
#define HDA_VERB_GET_GPI_CHANGE     0xF13u
#define HDA_VERB_GET_GPO_DATA       0xF14u
#define HDA_VERB_GET_GPIO_DATA      0xF15u
#define HDA_VERB_GET_GPIO_ENABLE    0xF16u
#define HDA_VERB_GET_GPIO_DIR       0xF17u
#define HDA_VERB_GET_GPIO_WAKE      0xF18u
#define HDA_VERB_GET_GPIO_STICKY    0xF19u
#define HDA_VERB_GET_GPIO_CHANGE    0xF1Au
#define HDA_VERB_GET_CONFIG_DEFAULT 0xF1Cu
#define HDA_VERB_GET_STRIPE_CTL     0xF24u
#define HDA_VERB_GET_ASP_CHAN_MAP   0xF34u
#define HDA_VERB_GET_SUBSYS_ID      0xF20u

#define HDA_VERB4_SET_CONV_FMT      0x2u
#define HDA_VERB4_SET_AMP_GAIN      0x3u

#define HDA_VERB_SET_CONN_SEL       0x701u
#define HDA_VERB_SET_POWER_STATE    0x705u
#define HDA_VERB_SET_CONV_STREAM    0x706u
#define HDA_VERB_SET_PIN_CTRL       0x707u
#define HDA_VERB_SET_UNSOL_RESP     0x708u
#define HDA_VERB_SET_PIN_SENSE      0x709u
#define HDA_VERB_SET_EAPD_BTL       0x70Cu
#define HDA_VERB_SET_HDMI_ELDD      0x70Du
#define HDA_VERB_SET_STRIPE_CTL     0x724u
#define HDA_VERB_SET_ASP_CHAN_MAP   0x734u
#define HDA_VERB_FUNC_RESET         0x7FFu

#define HDA_PARAM_VENDOR_ID         0x00u
#define HDA_PARAM_REVISION_ID       0x02u
#define HDA_PARAM_NODE_COUNT        0x04u
#define HDA_PARAM_FUNC_GROUP_TYPE   0x05u
#define HDA_PARAM_AUDIO_FUNC_CAP    0x08u
#define HDA_PARAM_AUDIO_WCAP        0x09u
#define HDA_PARAM_PCM_SIZES         0x0Au
#define HDA_PARAM_STREAM_FMTS       0x0Bu
#define HDA_PARAM_PIN_CAP           0x0Cu
#define HDA_PARAM_AMP_CAP_IN        0x0Du
#define HDA_PARAM_CONN_LIST_LEN     0x0Eu
#define HDA_PARAM_POWER_STATES      0x0Fu
#define HDA_PARAM_PROC_CAP          0x10u
#define HDA_PARAM_GPIO_COUNT        0x11u
#define HDA_PARAM_AMP_CAP_OUT       0x12u
#define HDA_PARAM_VOL_KNOB_CAP      0x13u

#define HDA_WCAP_TYPE_SHIFT         20u
#define HDA_WCAP_TYPE_MASK          (0xFu << 20)
#define HDA_WCAP_CHAN_COUNT_EXT     (1u << 13)
#define HDA_WCAP_CP_CAPS            (1u << 12)
#define HDA_WCAP_L_R_SWAP           (1u << 11)
#define HDA_WCAP_POWER_CTL          (1u << 10)
#define HDA_WCAP_DIGITAL            (1u << 9)
#define HDA_WCAP_CONN_LIST          (1u << 8)
#define HDA_WCAP_UNSOL_CAP          (1u << 7)
#define HDA_WCAP_PROC_WIDGET        (1u << 6)
#define HDA_WCAP_STRIPE             (1u << 5)
#define HDA_WCAP_FORMAT_OVR         (1u << 4)
#define HDA_WCAP_AMP_OVRD           (1u << 3)
#define HDA_WCAP_OUT_AMP            (1u << 2)
#define HDA_WCAP_IN_AMP             (1u << 1)
#define HDA_WCAP_STEREO             (1u << 0)

#define HDA_WID_TYPE_AUD_OUT        0x0u
#define HDA_WID_TYPE_AUD_IN         0x1u
#define HDA_WID_TYPE_AUD_MIX        0x2u
#define HDA_WID_TYPE_AUD_SEL        0x3u
#define HDA_WID_TYPE_PIN            0x4u
#define HDA_WID_TYPE_POWER          0x5u
#define HDA_WID_TYPE_VOL_KNOB       0x6u
#define HDA_WID_TYPE_BEEP_GEN       0x7u
#define HDA_WID_TYPE_VENDOR         0xFu

#define HDA_AMPCAP_MUTE             (1u << 31)
#define HDA_AMP_CAP_STEPSIZE_SHIFT  16u
#define HDA_AMP_CAP_STEPSIZE_MASK   (0x7Fu << 16)
#define HDA_AMPCAP_NUM_STEPS_SHIFT  8u
#define HDA_AMPCAP_NUM_STEPS_MASK   (0x7Fu << 8)
#define HDA_AMP_CAP_OFFSET_SHIFT    0u
#define HDA_AMP_CAP_OFFSET_MASK     0x7Fu

#define HDA_AMP_SET_OUTPUT          (1u << 15)
#define HDA_AMP_SET_INPUT           (1u << 14)
#define HDA_AMP_SET_LEFT            (1u << 13)
#define HDA_AMP_SET_RIGHT           (1u << 12)
#define HDA_AMP_SET_INDEX_SHIFT     8u
#define HDA_AMP_SET_INDEX_MASK      (0xFu << 8)
#define HDA_AMP_SET_MUTE            (1u << 7)
#define HDA_AMP_SET_GAIN_MASK       0x7Fu

#define HDA_PINCAP_IMP_SENSE        (1u << 0)
#define HDA_PINCAP_TRIG_REQ         (1u << 1)
#define HDA_PINCAP_PRES_DETECT      (1u << 2)
#define HDA_PINCAP_HP_DRV           (1u << 3)
#define HDA_PINCAP_OUT_CAP          (1u << 4)
#define HDA_PINCAP_IN_CAP           (1u << 5)
#define HDA_PINCAP_BAL_IO           (1u << 6)
#define HDA_PINCAP_HDMI             (1u << 7)
#define HDA_PINCAP_VREF_MASK        (0x7u << 8)
#define HDA_PINCAP_EAPD             (1u << 16)
#define HDA_PINCAP_DP               (1u << 24)
#define HDA_PINCAP_HBR              (1u << 27)

#define HDA_PINCTL_VREFEN_MASK      (0x7u << 0)
#define HDA_PINCTL_IN_EN            (1u << 5)
#define HDA_PINCTL_OUT_EN           (1u << 6)
#define HDA_PINCTL_HP_EN            (1u << 7)

#define HDA_EAPD_BTL_EAPD           (1u << 1)
#define HDA_EAPD_BTL_LR_SWAP        (1u << 0)

#define HDA_CFG_PORT_CONN_SHIFT     30u
#define HDA_CFG_PORT_CONN_JACK      0u
#define HDA_CFG_PORT_CONN_NONE      1u
#define HDA_CFG_PORT_CONN_FIXED     2u
#define HDA_CFG_PORT_CONN_BOTH      3u
#define HDA_CFG_LOCATION_SHIFT      24u
#define HDA_CFG_DEF_DEVICE_SHIFT    20u
#define HDA_CFG_DEF_DEVICE_MASK     (0xFu << 20)
#define HDA_CFG_DEVICE_HP_OUT       0x2u
#define HDA_CFG_DEVICE_SPEAKER      0x3u
#define HDA_CFG_DEVICE_CD           0x4u
#define HDA_CFG_DEVICE_SPDIF_OUT    0x5u
#define HDA_CFG_DEVICE_DIG_OUT      0x6u
#define HDA_CFG_DEVICE_MIC_IN       0x8u
#define HDA_CFG_DEVICE_AUX          0x9u
#define HDA_CFG_DEVICE_LINE_IN      0xAu
#define HDA_CFG_DEVICE_SPDIF_IN     0xCu
#define HDA_CFG_CONNECT_TYPE_SHIFT  16u
#define HDA_CFG_COLOR_SHIFT         12u
#define HDA_CFG_MISC_SHIFT          8u
#define HDA_CFG_ASSOC_SHIFT         4u
#define HDA_CFG_SEQ_SHIFT           0u

#define HDA_POWER_D0                0x00u
#define HDA_POWER_D1                0x01u
#define HDA_POWER_D2                0x02u
#define HDA_POWER_D3HOT             0x03u
#define HDA_POWER_D3COLD            0x04u

typedef struct __attribute__((packed)) {
    uint32_t  addr_lo;
    uint32_t  addr_hi;
    uint32_t  length;
    uint32_t  flags;
} hda_bdl_entry_t;

typedef struct __attribute__((packed)) {
    uint32_t  response;
    uint32_t  ex_resp;
} hda_rirb_entry_t;

typedef struct {
    uint32_t  sample_rate;
    uint8_t   channels;
    uint8_t   bits;
    uint8_t   is_float;
} hda_pcm_format_t;

typedef struct {
    uint8_t   nid;
    uint8_t   type;
    uint32_t  wcap;
    uint32_t  pincap;
    uint32_t  amp_cap_in;
    uint32_t  amp_cap_out;
    uint32_t  cfg_default;
    uint32_t  supported_rates;
    uint32_t  supported_fmts;
    uint8_t   conn_list[HDA_MAX_CONNS];
    uint8_t   conn_count;
    uint8_t   sel_conn;
    uint8_t   power_state;
    uint8_t   pin_ctl;
    int8_t    vol_left;
    int8_t    vol_right;
    uint8_t   muted;
    uint8_t   valid;
} hda_widget_t;

typedef struct {
    uint8_t   dac_nid;
    uint8_t   pin_nid;
    uint8_t   path[16];
    uint8_t   path_len;
    uint8_t   is_headphone;
    uint8_t   is_speaker;
    uint8_t   active;
} hda_audio_path_t;

typedef struct {
    uint8_t   addr;
    uint16_t  vendor_id;
    uint16_t  device_id;
    uint32_t  revision;
    uint32_t  subsys_id;
    uint8_t   afg_nid;
    uint8_t   node_start;
    uint8_t   node_count;
    hda_widget_t     widgets[HDA_MAX_WIDGETS];
    hda_audio_path_t paths[HDA_MAX_AUDIO_PATHS];
    uint8_t          path_count;
    uint8_t          valid;
} hda_codec_t;

typedef struct hda_stream hda_stream_t;

struct hda_stream {
    uint8_t   idx;
    uint8_t   strm_num;
    uint8_t   is_output;
    uint8_t   running;
    hda_pcm_format_t  fmt;
    uint32_t  buf_size;
    uint32_t  period_size;
    uint32_t  periods;
    uint32_t  write_pos;
    uint32_t  read_pos;
    uint8_t  *dma_buf;
    uint64_t  dma_buf_phys;
    hda_bdl_entry_t *bdl;
    uint64_t  bdl_phys;
    void    (*period_cb)(hda_stream_t *s, void *priv);
    void     *cb_priv;
};

typedef struct {
    int8_t    vol_left;
    int8_t    vol_right;
    uint8_t   muted;
    uint8_t   active;
    char      name[32];
} hda_mixer_channel_t;

typedef struct {
    uint32_t  freq_hz;
    int32_t   gain_db_x10;
    uint32_t  q_x10;
    uint8_t   enabled;
} hda_eq_band_t;

typedef enum {
    HDA_OK                  =  0,
    HDA_ERR_NOT_FOUND       = -1,
    HDA_ERR_RESET           = -2,
    HDA_ERR_NO_CODECS       = -3,
    HDA_ERR_VERB_TIMEOUT    = -4,
    HDA_ERR_INVAL           = -5,
    HDA_ERR_BUSY            = -6,
    HDA_ERR_NO_PATH         = -7,
    HDA_ERR_FMT             = -8,
    HDA_ERR_DMA             = -9,
    HDA_ERR_NOMEM           = -10,
} hda_status_t;

hda_status_t  hda_init(pci_device_t *pdev);
hda_stream_t *hda_open_output(const hda_pcm_format_t *fmt,
                               void (*period_cb)(hda_stream_t *, void *),
                               void *priv);
hda_stream_t *hda_open_input(const hda_pcm_format_t *fmt,
                              void (*period_cb)(hda_stream_t *, void *),
                              void *priv);
hda_status_t  hda_stream_start(hda_stream_t *s);
hda_status_t  hda_stream_stop(hda_stream_t *s);
hda_status_t  hda_stream_close(hda_stream_t *s);
uint32_t      hda_stream_write(hda_stream_t *s, const void *data, uint32_t len);
uint32_t      hda_stream_avail(const hda_stream_t *s);
uint32_t      hda_stream_lpib(const hda_stream_t *s);
hda_status_t  hda_set_master_vol(uint8_t vol_pct);
hda_status_t  hda_set_master_mute(int mute);
hda_status_t  hda_set_channel_vol(uint8_t ch, uint8_t left, uint8_t right);
hda_status_t  hda_set_eq_band(uint8_t band, uint32_t freq_hz,
                               int32_t gain_db_x10, uint32_t q_x10);
hda_status_t  hda_eq_enable(int enable);
uint8_t       hda_codec_count(void);
const hda_codec_t *hda_get_codec(uint8_t idx);
void          hda_irq_handler(void);
const char   *hda_strerror(hda_status_t st);

#ifndef NDEBUG
void hda_debug_dump(void);
#else
static inline void hda_debug_dump(void) {}
#endif

#endif
