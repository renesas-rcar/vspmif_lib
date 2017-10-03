/* 
 * Copyright (c) 2015-2016 Renesas Electronics Corporation
 * Released under the MIT license
 * http://opensource.org/licenses/mit-license.php 
 */

#ifndef __VSPM_PUBLIC_H__
#define __VSPM_PUBLIC_H__

#include "vsp_drv.h"
#include "fdp_drv.h"
#include "vspm_cmn.h"

/* callback function */
typedef void (*PFN_VSPM_COMPLETE_CALLBACK)(
	unsigned long job_id, long result, void *user_data);

/* VSP Manager APIs */
long vspm_init_driver(
		void **handle,
		struct vspm_init_t *param);

long vspm_quit_driver(
		void *handle);

long vspm_entry_job(
		void *handle,
		unsigned long *job_id,
		char job_priority,
		struct vspm_job_t *ip_param,
		void *user_data,
		PFN_VSPM_COMPLETE_CALLBACK cb_func);

long vspm_cancel_job(
		void *handle,
		unsigned long job_id);

long vspm_get_status(
		void *handle,
		struct vspm_status_t *status);

/* R-Car H2/M2 compatible VSP params */
#define VSP_NO_DITHER				(0x00)
#define VSP_DITHER					(0x03)

#define VSP_FMD_NO					(0x00)
#define VSP_FMD						(0x01)

#define VSP_QNT_OFF					(0x00)
#define VSP_QNT_ON					(0x01)

typedef struct {
	unsigned long *clut;
	short size;
} T_VSP_OSDLUT;

typedef struct {
	void *addr_a;
	unsigned char alphan;
	unsigned long alpha1;
	unsigned long alpha2;
	unsigned short astride;
	unsigned char aswap;
	unsigned char asel;
	unsigned char aext;
	unsigned char anum0;
	unsigned char anum1;
	unsigned char afix;
	unsigned char irop;
	unsigned char msken;
	unsigned char bsel;
	unsigned long mgcolor;
	unsigned long mscolor0;
	unsigned long mscolor1;
} T_VSP_ALPHA;

typedef struct {
	unsigned long color1;
	unsigned long color2;
} T_VSP_CLRCNV;

typedef struct {
	void *addr;					/* Y or RGB buffer address */
	void *addr_c0;				/* CbCr or CB buffer address */
	void *addr_c1;				/* Cr buffer address */
	unsigned short stride;
	unsigned short stride_c;
	unsigned short width;
	unsigned short height;
	unsigned short width_ex;
	unsigned short height_ex;
	unsigned short x_offset;
	unsigned short y_offset;
	unsigned short format;
	unsigned char swap;
	unsigned short x_position;
	unsigned short y_position;
	unsigned char pwd;
	unsigned char cipm;
	unsigned char cext;
	unsigned char csc;
	unsigned char iturbt;
	unsigned char clrcng;
	unsigned char vir;
	unsigned long vircolor;
	T_VSP_OSDLUT *osd_lut;
	T_VSP_ALPHA *alpha_blend;
	T_VSP_CLRCNV *clrcnv;
	unsigned long connect;
} T_VSP_IN;

typedef struct {
	void *addr;					/* Y or RGB buffer address */
	void *addr_c0;				/* CbCr or CB buffer address */
	void *addr_c1;				/* Cr buffer address */
	unsigned short stride;
	unsigned short stride_c;
	unsigned short width;
	unsigned short height;
	unsigned short x_offset;
	unsigned short y_offset;
	unsigned short format;
	unsigned char swap;
	unsigned char pxa;
	unsigned char pad;
	unsigned short x_coffset;
	unsigned short y_coffset;
	unsigned char csc;
	unsigned char iturbt;
	unsigned char clrcng;
	unsigned char cbrm;
	unsigned char abrm;
	unsigned char athres;
	unsigned char clmd;
	unsigned char ln16;			/* no support */
	unsigned char dith;
	unsigned char rotation;		/* no support */
	unsigned char mirror;		/* no support */
} T_VSP_OUT;

/* SRU parameter */
typedef struct {
	unsigned char mode;
	unsigned char param;
	unsigned short enscl;
	unsigned char fxa;
	unsigned long connect;
} T_VSP_SRU;

/* UDS parameter */
typedef struct{
	unsigned char amd;
	unsigned char fmd;
	unsigned long filcolor;
	unsigned char clip;
	unsigned char alpha;
	unsigned char complement;
	unsigned char athres0;
	unsigned char athres1;
	unsigned char anum0;
	unsigned char anum1;
	unsigned char anum2;
	unsigned short x_ratio;
	unsigned short y_ratio;
	unsigned short x_stp;		/* no support */
	unsigned short x_edp;		/* no support */
	unsigned short y_stp;		/* no support */
	unsigned short y_edp;		/* no support */
	unsigned short in_cwidth;	/* no support */
	unsigned short in_cheight;	/* no support */
	unsigned short x_coffset;	/* no support */
	unsigned short y_coffset;	/* no support */
	unsigned short out_cwidth;
	unsigned short out_cheight;
	unsigned long connect;
} T_VSP_UDS;

/* LUT parameter */
typedef struct {
	unsigned long *lut;
	short size;
	unsigned char fxa;
	unsigned long connect;
} T_VSP_LUT;

/* CLU parameter */
typedef struct {
	unsigned char mode;
	unsigned long *clu_addr;
	unsigned long *clu_data;
	short size;
	unsigned char fxa;
	unsigned long connect;
} T_VSP_CLU;

/* HST parameter */
typedef struct {
	unsigned char fxa;
	unsigned long connect;
} T_VSP_HST;

/* HSI parameter */
typedef struct {
	unsigned char fxa;
	unsigned long connect;
} T_VSP_HSI;

/* BRU parameter */
typedef struct {
	unsigned short width;
	unsigned short height;
	unsigned short x_position;
	unsigned short y_position;
	unsigned char pwd;
	unsigned long color;
} T_VSP_BLEND_VIRTUAL;

typedef struct {
	unsigned char rbc;
	unsigned char crop;
	unsigned char arop;
	unsigned char blend_formula;
	unsigned char blend_coefx;
	unsigned char blend_coefy;
	unsigned char aformula;
	unsigned char acoefx;
	unsigned char acoefy;
	unsigned char acoefx_fix;
	unsigned char acoefy_fix;
} T_VSP_BLEND_CONTROL;

typedef struct {
	unsigned char crop;
	unsigned char arop;
} T_VSP_BLEND_ROP;

typedef struct {
	unsigned long lay_order;
	unsigned char adiv;
	unsigned char qnt[4];
	unsigned char dith[4];
	T_VSP_BLEND_VIRTUAL *blend_virtual;
	T_VSP_BLEND_CONTROL *blend_control_a;
	T_VSP_BLEND_CONTROL *blend_control_b;
	T_VSP_BLEND_CONTROL *blend_control_c;
	T_VSP_BLEND_CONTROL *blend_control_d;
	T_VSP_BLEND_ROP *blend_rop;
	unsigned long connect;
} T_VSP_BRU;

/* HGO parameter */
typedef struct {
	/* histogram detection window */
	void *addr;						/* use 768 bytes */
	unsigned short width;			/* horizontal size */
	unsigned short height;			/* vertical size */
	unsigned short x_offset;		/* horizontal offset */
	unsigned short y_offset;		/* vertical offset*/
	unsigned char binary_mode;		/* offset binary mode */
	unsigned char maxrgb_mode;		/* source component setting */
	unsigned short x_skip;			/* horizontal pixel skipping mode */
	unsigned short y_skip;			/* vertical pixel skipping mode */

	unsigned long sampling;			/* sampling module */
} T_VSP_HGO;

/* HGT parameter */
typedef struct {
	unsigned char lower;			/* lower boundary value */
	unsigned char upper;			/* upper boundary value */
} T_VSP_HUE_AREA;

typedef struct {
	/* histogram detection window */
	void *addr;						/* use 768 bytes */
	unsigned short width;			/* horizontal size */
	unsigned short height;			/* vertical size */
	unsigned short x_offset;		/* horizontal offset */
	unsigned short y_offset;		/* vertical offset*/
	unsigned short x_skip;			/* horizontal pixel skipping mode */
	unsigned short y_skip;			/* vertical pixel skipping mode */
	T_VSP_HUE_AREA area[6];			/* hue area */

	unsigned long sampling;			/* sampling module */
} T_VSP_HGT;

typedef struct {
	T_VSP_SRU *sru;					/* super-resolution */
	T_VSP_UDS *uds;					/* up down scaler */
	T_VSP_UDS *uds1;				/* up down scaler */
	T_VSP_UDS *uds2;				/* up down scaler */
	T_VSP_LUT *lut;					/* look up table */
	T_VSP_CLU *clu;					/* cubic look up table */
	T_VSP_HST *hst;					/* hue saturation value transform */
	T_VSP_HSI *hsi;					/* hue saturation value inverse */
	T_VSP_BRU *bru;					/* blend rop */
	T_VSP_HGO *hgo;					/* histogram generator-one */
	T_VSP_HGT *hgt;					/* histogram generator-two */
} T_VSP_CTRL;

typedef struct {
	unsigned char rpf_num;			/* RPF number */
	unsigned long rpf_order;		/* RPF order */
	unsigned long use_module;		/* using module */
	T_VSP_IN *src1_par;				/* source1 parameter */
	T_VSP_IN *src2_par;				/* source2 parameter */
	T_VSP_IN *src3_par;				/* source3 parameter */
	T_VSP_IN *src4_par;				/* source4 parameter */
	T_VSP_OUT *dst_par;				/* destination parameter */
	T_VSP_CTRL *ctrl_par;			/* module parameter */
} T_VSP_START;

/* R-Car H2/M2 compatible FDP params */
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

/* R-Car H2/M2 compatible defines */
#define R_FDPM_OK		(0)
#define R_FDPM_NG		(-1)

enum {
	E_FDP_END = 0,
	E_FDP_DELAYED,
};

typedef struct {
	T_FDP_IMGBUF *buf_in;
	T_FDP_IMGSIZE *insize;
	T_FDP_IMGSIZE *refsize;
	T_FDP_IMGSIZE *iirsize;
	T_FDP_IMGSIZE *outsize;
	unsigned char refwr_num;
	unsigned char refrd0_num;
	unsigned char refrd1_num;
	unsigned char refrd2_num;
	unsigned char refwr_y_en;
	unsigned char refwr_c_en;
	unsigned char refrd0_en;
	unsigned char refrd1_en;
	unsigned char refrd2_en;
	unsigned char refiir_en;
	void *userdata1;
} T_FDP_CB1;

typedef struct {
	int ercd;
	void *userdata2;
} T_FDP_CB2;

typedef void			(*FP)(void);
typedef T_VSP_START		VSPM_VSP_PAR;
typedef void			VSPM_2DDMAC_PAR;

typedef struct {
	unsigned short uhType;
	union {
		VSPM_VSP_PAR *ptVsp;
		VSPM_2DDMAC_PAR *pt2dDmac;
	} unionIpParam;
} VSPM_IP_PAR;

/* R-Car H2/M2 compatible APIs */
long VSPM_lib_DriverInitialize(
	unsigned long *handle);

long VSPM_lib_DriverQuit(
	unsigned long handle);

long VSPM_lib_Entry(
	unsigned long handle,
	unsigned long *puwJobId,
	char bJobPriority,
	VSPM_IP_PAR *ptIpParam,
	unsigned long uwUserData,
	void *pfnNotifyComplete);

long VSPM_lib_Cancel(
	unsigned long handle,
	unsigned long uwJobId);

short VSPM_lib_PVioUdsGetRatio(
	unsigned short ushInSize,
	unsigned short ushOutSize,
	unsigned short *ushRatio);

int drv_FDPM_Open(
	void *callback2,
	void *callback3,
	void *callback4,
	T_FDP_OPEN *open_par,
	FP user_function,
	void *userdata2,
	void *userdata3,
	void *userdata4,
	int *sub_ercd);

int drv_FDPM_Close(
	FP user_function,
	int *sub_ercd,
	unsigned char f_release);

int drv_FDPM_Start(
	void *callback1,
	void *callback2,
	T_FDP_START *start_par,
	void *userdata1,
	void *userdata2,
	int *sub_ercd);

int drv_FDPM_Cancel(
	int id,
	int *sub_ercd);

int drv_FDPM_Status(
	T_FDP_STATUS *fdp_status,
	int *sub_ercd);

#endif	/* __VSPM_PUBLIC_H__ */
