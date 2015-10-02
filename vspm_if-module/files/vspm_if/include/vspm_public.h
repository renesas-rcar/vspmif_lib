/* 
 * Copyright (c) 2015 Renesas Electronics Corporation
 * Released under the MIT license
 * http://opensource.org/licenses/mit-license.php 
 */

#ifndef __VSPM_PUBLIC_H__
#define __VSPM_PUBLIC_H__

#include "vsp_drv.h"
#include "fdp_drv.h"

/* VSP Manager APIs return codes */
#define R_VSPM_OK			(0)
#define	R_VSPM_NG			(-1)	/* abnormal termination */
#define	R_VSPM_PARAERR		(-2)	/* illegal parameter */
#define	R_VSPM_SEQERR		(-3)	/* sequence error */
#define R_VSPM_QUE_FULL		(-4)	/* request queue full */
#define R_VSPM_CANCEL		(-5)	/* processing was canceled */
#define R_VSPM_ALREADY_USED	(-6)	/* already used all channel */
#define R_VSPM_OCCUPY_CH	(-7)	/* occupy channel */
#define R_VSPM_DRIVER_ERR	(-10)	/* IP error(in driver) */

/* using channel */
#define VSPM_EMPTY_CH		(0xFFFFFFFF)
#define VSPM_USE_CH0		(0x00000001)
#define VSPM_USE_CH1		(0x00000002)
#define VSPM_USE_CH2		(0x00000004)
#define VSPM_USE_CH3		(0x00000008)
#define VSPM_USE_CH4		(0x00000010)
#define VSPM_USE_CH5		(0x00000020)
#define VSPM_USE_CH6		(0x00000040)
#define VSPM_USE_CH7		(0x00000080)

/* operation mode */
enum {
	VSPM_MODE_MUTUAL = 0,
	VSPM_MODE_OCCUPY
};

/* select IP */
enum {
	VSPM_TYPE_VSP_AUTO = 0,
	VSPM_TYPE_FDP_AUTO
};

/* Job priority */
#define VSPM_PRI_MAX		((char)126)
#define VSPM_PRI_MIN		((char)  1)

/* State of the entry */
#define VSPM_STATUS_WAIT		1
#define VSPM_STATUS_ACTIVE		2
#define VSPM_STATUS_NO_ENTRY	3

/* Renesas near-lossless compression setting */
#define FCP_FCNL_DISABLE		(0)
#define FCP_FCNL_ENABLE			(1)

/* Tile/Linear conversion setting */
#define FCP_TL_DISABLE			(0)
#define FCP_TL_ENABLE			(1)

/* callback function */
typedef void (*PFN_VSPM_COMPLETE_CALLBACK)(
	unsigned long job_id, long result, unsigned long user_data);

struct vspm_init_vsp_t {
	/* reserved */
};

struct vspm_init_fdp_t {
	void *hard_addr[2];
};

/* initialize parameter structure */
struct vspm_init_t {
	unsigned int use_ch;
	unsigned short mode;
	unsigned short type;
	union {
		struct vspm_init_vsp_t *vsp;
		struct vspm_init_fdp_t *fdp;
	} par;
};

/* entry parameter structure */
struct vspm_job_t {
	unsigned short type;
	union {
		struct vsp_start_t *vsp;
		struct fdp_start_t *fdp;
	} par;
};

/* status parameter structure */
struct vspm_status_t {
	struct fdp_status_t *fdp;
};

/* FCP information structure */
struct fcp_info_t {
	unsigned char fcnl;
	unsigned char tlen;
	unsigned short pos_y;
	unsigned short pos_c;
	unsigned short stride_div16;
	void *ba_anc_prev_y;
	void *ba_anc_cur_y;
	void *ba_anc_next_y;
	void *ba_anc_cur_c;
	void *ba_ref_prev_y;
	void *ba_ref_cur_y;
	void *ba_ref_next_y;
	void *ba_ref_cur_c;
};

/* VSP Manager APIs */
long vspm_init_driver(
	unsigned long *handle,
	struct vspm_init_t *param);

long vspm_quit_driver(
	unsigned long handle);

long vspm_entry_job(
	unsigned long handle,
	unsigned long *job_id,
	char job_priority,
	struct vspm_job_t *ip_param,
	unsigned long user_data,
	PFN_VSPM_COMPLETE_CALLBACK cb_func);

long vspm_cancel_job(
	unsigned long handle,
	unsigned long job_id);

long vspm_get_status(
	unsigned long handle,
	struct vspm_status_t *status);

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
	PFN_VSPM_COMPLETE_CALLBACK pfnNotifyComplete);

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
