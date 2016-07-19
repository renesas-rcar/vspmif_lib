/* 
 * Copyright (c) 2015-2016 Renesas Electronics Corporation
 * Released under the MIT license
 * http://opensource.org/licenses/mit-license.php 
 */

#ifndef __FDP_DRV_H__
#define __FDP_DRV_H__

#define E_FDP_PARA_REFBUF		(-253)
#define E_FDP_PARA_STARTPAR		(-300)
#define E_FDP_PARA_FDPGO		(-301)
#define E_FDP_PARA_FPROCPAR		(-302)
#define E_FDP_PARA_SEQPAR		(-303)
#define E_FDP_PARA_INPIC		(-305)
#define E_FDP_PARA_OUTBUF		(-306)
#define E_FDP_PARA_SEQMODE		(-307)
#define E_FDP_PARA_TELECINEMODE	(-308)
#define E_FDP_PARA_INWIDTH		(-309)
#define E_FDP_PARA_INHEIGHT		(-310)
#define E_FDP_PARA_PICWIDTH		(-314)
#define E_FDP_PARA_PICHEIGHT	(-315)
#define E_FDP_PARA_CHROMA		(-316)
#define E_FDP_PARA_PROGSEQ		(-317)
#define E_FDP_PARA_PICSTRUCT	(-318)
#define E_FDP_PARA_REPEATTOP	(-319)
#define E_FDP_PARA_BUFREFRD0	(-321)
#define E_FDP_PARA_BUFREFRD1	(-322)
#define E_FDP_PARA_BUFREFRD2	(-323)
#define E_FDP_PARA_LASTSTART	(-329)
#define E_FDP_PARA_CF			(-330)
#define E_FDP_PARA_INTERPOLATED	(-331)
#define E_FDP_PARA_OUTFORMAT	(-332)
#define E_FDP_PARA_SRC_ADDR		(-350)
#define E_FDP_PARA_SRC_ADDR_C0	(-351)
#define E_FDP_PARA_SRC_ADDR_C1	(-352)
#define E_FDP_PARA_SRC_STRIDE	(-353)
#define E_FDP_PARA_SRC_STRIDE_C	(-354)
#define E_FDP_PARA_DST_ADDR		(-355)
#define E_FDP_PARA_DST_ADDR_C0	(-356)
#define E_FDP_PARA_DST_ADDR_C1	(-357)
#define E_FDP_PARA_DST_STRIDE	(-358)
#define E_FDP_PARA_DST_STRIDE_C	(-359)
#define E_FDP_PARA_STLMSK_ADDR	(-360)
#define E_FDP_PARA_FCNL			(-400)
#define E_FDP_PARA_TLEN			(-401)
#define E_FDP_PARA_FCP_POS		(-402)
#define E_FDP_PARA_FCP_STRIDE	(-403)
#define E_FDP_PARA_BA_ANC		(-404)
#define E_FDP_PARA_BA_REF		(-405)

/* FDP processing status */
enum {
	FDP_STAT_NO_INIT = 0,
	FDP_STAT_INIT,
	FDP_STAT_READY,
	FDP_STAT_RUN,
};

/* FDP processing opration parameter */
enum {
	FDP_NOGO = 0,
	FDP_GO,
};

/* Current field parameter */
enum {
	FDP_CF_TOP = 0,
	FDP_CF_BOTTOM,
};

/* De-interlacing mode parameter */
enum {
	FDP_DIM_PREV = 3,	/* Select previous field */
	FDP_DIM_NEXT,		/* Select next field */
};

/* Input/Output format */
enum {
	FDP_YUV420 = 0,
	FDP_YUV420_PLANAR,
	FDP_YUV420_NV21,
	FDP_YUV422_NV16,
	FDP_YUV422_YUY2,
	FDP_YUV422_UYVY,
	FDP_YUV422_PLANAR,
	FDP_YUV444_PLANAR,
};
#define FDP_YUV420_YV12			FDP_YUV420_PLANAR
#define FDP_YUV420_YU12			FDP_YUV420_PLANAR
#define FDP_YUV422_YV16			FDP_YUV422_PLANAR

/* Sequence mode parameter */
enum {
	FDP_SEQ_PROG = 0,		/* Progressive */
	FDP_SEQ_INTER,			/* Interlace Adaptive 3D/2D */
	FDP_SEQ_INTERH,			/* not used */
	FDP_SEQ_INTER_2D,		/* Interlace Fixed 2D */
	FDP_SEQ_INTERH_2D,		/* not used */
};

/* Telecine mode parameter */
enum {
	FDP_TC_OFF = 0,				/* Disable */
	FDP_TC_ON,					/* not used */
	FDP_TC_FORCED_PULL_DOWN,	/* Force 2-3 pull down mode */
	FDP_TC_INTERPOLATED_LINE,	/* Interpolated line mode */
};

/* Sequence information parameter */
struct fdp_seq_t {
	unsigned char seq_mode;
	unsigned char telecine_mode;
	unsigned short in_width;
	unsigned short in_height;
};

/* Picture information structure */
struct fdp_pic_t {
	unsigned long picid;
	unsigned char chroma_format;	/* input format */
	unsigned short width;			/* picture horizontal size */
	unsigned short height;			/* picture vertical size */
	unsigned char progressive_sequence;
	unsigned char progressive_frame;
	unsigned char picture_structure;
	unsigned char repeat_first_field;
	unsigned char top_field_first;
};

/* Picture image buffer information structure */
struct fdp_imgbuf_t {
	unsigned int addr;
	unsigned int addr_c0;
	unsigned int addr_c1;
	unsigned short stride;
	unsigned short stride_c;
};

/* Reference buffer information structure */
struct fdp_refbuf_t {
	struct fdp_imgbuf_t *next_buf;
	struct fdp_imgbuf_t *cur_buf;
	struct fdp_imgbuf_t *prev_buf;
};

/* De-interlace information structure */
struct fdp_ipc_t {
	unsigned char cmb_ofst;
	unsigned char cmb_max;
	unsigned char cmb_gard;
};

/* Processing information structure */
struct fdp_fproc_t {
	struct fdp_seq_t *seq_par;
	struct fdp_pic_t *in_pic;
	unsigned char last_seq_indicator;
	unsigned char current_field;
	unsigned char interpolated_line;
	unsigned char out_format;
	struct fdp_imgbuf_t *out_buf;
	struct fdp_refbuf_t *ref_buf;
	struct fcp_info_t *fcp_par;
	struct fdp_ipc_t *ipc_par;
};

/* Start information parameter */
struct fdp_start_t {
	unsigned char fdpgo;
	struct fdp_fproc_t *fproc_par;
};

/* Status information parameter */
struct fdp_status_t {
	unsigned long picid;
	unsigned int vcycle;
	unsigned int sensor[18];
};

/* R-Car H2/M2 compatible parameter */
#define E_FDP_PARA_VMODE		(-208)

enum {
	FDP_OCMODE_OCCUPY = 0,
	FDP_OCMODE_COMMON,
};

enum {
	FDP_VMODE_NORMAL = 0,
	FDP_VMODE_VBEST,
	FDP_VMODE_VBEST_FDP0,
	FDP_VMODE_VBEST_FDP1,
	FDP_VMODE_VBEST_FDP2,
};

enum {
	FDP_CLKMODE_1 = 1,
};

enum {
	FDPM_IDLE = 0,
	FDPM_RDY,
	FDPM_SHARE_BUSY,
	FDPM_OCCUPY_BUSY,
	FDPM_FULL_BUSY,
};

enum {
	FDP_SEQ_UNLOCK = 0,
	FDP_SEQ_LOCK,
};

enum {
	FDP_IN_DISABLE = 0,
	FDP_IN_ENABLE,
};

enum {
	FDP_OUT_DISABLE = 0,
	FDP_OUT_ENABLE,
};

enum {
	FDP_OUT_NOREQ = 0,
	FDP_OUT_REQ,
};

typedef struct {
	void *addr;
	void *addr_c0;
	void *addr_c1;
	unsigned short stride;
	unsigned short stride_c;
	unsigned short height;
	unsigned short height_c;
} T_FDP_IMGBUF;

typedef struct {
	T_FDP_IMGBUF *buf_ref0;
	T_FDP_IMGBUF *buf_ref1;
	T_FDP_IMGBUF *buf_ref2;
	T_FDP_IMGBUF *buf_refprg;
	void *stlmsk_adr;
	T_FDP_IMGBUF *buf_iir;
} T_FDP_REFPREBUF;

typedef struct {
	unsigned short width;
	unsigned short height;
} T_FDP_IMGSIZE;

typedef struct {
	unsigned char ref_mode;
	unsigned char refbuf_mode;
	T_FDP_REFPREBUF *refbuf;
	unsigned char ocmode;
	unsigned char vmode;
	T_FDP_IMGSIZE *insize;
	unsigned char clkmode;
	unsigned long vcnt;
} T_FDP_OPEN;

typedef struct {
	unsigned char ratio_type;
	short h_iniphase;
	short h_endphase;
	unsigned short h_ratio;
	short v_iniphase;
	short v_endphase;
	unsigned short v_ratio;
} T_FDP_RATIO;

typedef struct {
	unsigned char seq_mode;
	unsigned char scale_mode;
	unsigned char filter_mode;
	unsigned char telecine_mode;
	unsigned short in_width;
	unsigned short in_height;
	unsigned short imgleft;
	unsigned short imgtop;
	unsigned short imgwidth;
	unsigned short imgheight;
	unsigned short out_width;
	unsigned short out_height;
	T_FDP_RATIO *ratio;
} T_FDP_SEQ;

typedef struct {
	unsigned char dummy;
} T_FDP_IMGSET;

typedef struct {
	unsigned short width;
	unsigned short height;
	unsigned char chroma_format;
	unsigned char progressive_sequence;
	unsigned char progressive_frame;
	unsigned char picture_structure;
	unsigned char repeat_first_field;
	unsigned char top_field_first;
} T_FDP_PICPAR;

typedef struct {
	unsigned long picid;
	T_FDP_PICPAR *pic_par;
	T_FDP_IMGBUF *in_buf1;
	T_FDP_IMGBUF *in_buf2;
} T_FDP_PIC;

typedef struct {
	T_FDP_IMGBUF *buf_refwr;
	T_FDP_IMGBUF *buf_refrd0;
	T_FDP_IMGBUF *buf_refrd1;
	T_FDP_IMGBUF *buf_refrd2;
	T_FDP_IMGBUF *buf_iirwr;
	T_FDP_IMGBUF *buf_iirrd;
} T_FDP_REFBUF;

typedef struct {
	T_FDP_SEQ *seq_par;
	T_FDP_IMGSET *imgset_par;
	T_FDP_PIC *in_pic;
	unsigned char last_start;
	unsigned char cf;
	unsigned char f_decodeseq;
	T_FDP_IMGBUF *out_buf;
	unsigned char out_format;
	T_FDP_REFBUF *ref_buf;
} T_FDP_FPROC;

typedef struct {
	unsigned long *vcnt;
	unsigned char fdpgo;
	T_FDP_FPROC *fproc_par;
} T_FDP_START;

typedef struct {
	unsigned char status;
	unsigned long delay;
	unsigned long vcycle;
	unsigned long vintcnt;
	unsigned char seq_lock;
	unsigned char in_enable;
	unsigned long in_picid;
	unsigned char in_left;
	unsigned char out_enable;
	unsigned long out_picid;
	unsigned char out_left;
	unsigned char out_req;
} T_FDP_STATUS;
#endif
