/*
 * Copyright (c) 2018 Renesas Electronics Corporation
 * Released under the MIT license
 * http://opensource.org/licenses/mit-license.php
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pthread.h>

#include "vspm_public.h"
#include "mmngr_user_public.h"

#define FB_ENABLE		1	/* 0: disable output framebuffer, 1: enable */

int src_width	= 1280;
int src_height	= 720;
int src_size	= 1280 * 720 * 2;	/* yuv 422 planar */
int dst_width	= 1920;
int dst_height	= 1080;
int dst_size	= 1920 * 1080 * 4;	/* argb8888 */
int tbl_num		= 192 + 64 * 7;		/* roundup(1920/256)-1 == 7 ; max:192+64*31 */

struct vspm_tp_private_t {
	void *in_virt;
	unsigned int in_hard;
	MMNGR_ID in_fd;

	void *out_virt;
	unsigned int out_hard;
	MMNGR_ID out_fd;

	void *dl_virt;
	unsigned int dl_hard;
	MMNGR_ID dl_fd;

	void *handle;
};

struct vspm_tp_cb_info_t {
	unsigned long job_id;
	long ercd;

	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

/* release memory of memory manager */
static int release_memory(struct vspm_tp_private_t *priv)
{
	if (priv->dl_fd != 0)
		(void)mmngr_free_in_user_ext(priv->dl_fd);
	if (priv->out_fd != 0)
		(void)mmngr_free_in_user_ext(priv->out_fd);
	if (priv->in_fd != 0)
		(void)mmngr_free_in_user_ext(priv->in_fd);

	return 0;
}

/* allocate memory from memory manager */
static int allocate_memory(struct vspm_tp_private_t *priv)
{
	int ercd;

	/* input buffer */
	ercd = mmngr_alloc_in_user_ext(
		&priv->in_fd, src_size, &priv->in_hard, &priv->in_virt, MMNGR_VA_SUPPORT, NULL);
	if (ercd != R_MM_OK) {
		printf("Error: failed to allocate memory for input!! ercd=%d\n", ercd);
		return -1;
	}

	/* output buffer */
	ercd = mmngr_alloc_in_user_ext(
		&priv->out_fd, dst_size, &priv->out_hard, &priv->out_virt, MMNGR_VA_SUPPORT, NULL);
	if (ercd != R_MM_OK) {
		printf("Error: failed to allocate memory for output!! ercd=%d\n", ercd);
		(void)release_memory(priv);
		return -1;
	}
	memset(priv->out_virt, 0, dst_size);

	/* display list */
	ercd = mmngr_alloc_in_user_ext(
		&priv->dl_fd, tbl_num*8, &priv->dl_hard, &priv->dl_virt, MMNGR_VA_SUPPORT, NULL);
	if (ercd != R_MM_OK) {
		printf("Error: failed to allocate memory for DL!! ercd=%d\n", ercd);
		(void)release_memory(priv);
		return -1;
	}
	memset((void *)priv->dl_virt, 0, tbl_num*8);

	return 0;
}

/* create source image */
static void create_source_image(struct vspm_tp_private_t *priv)
{
	unsigned char *y, *u, *v;
	int row, col;
	int split;

	y = (unsigned char *)priv->in_virt;
	u = y + (src_width * src_height);
	v = u + (src_width * src_height / 2);

	split = src_height / 4;
	for (row = 0; row < split; row++) {
		for (col = 0; col < (src_width / 2); col++) {
			*y = 0x40;
			y++;
			*y = 0x40;
			y++;
			*u = 0x20;
			u++;
			*v = 0xf0;
			v++;
		}
	}
	for ( ; row < (split * 2); row++) {
		for (col = 0; col < (src_width / 2); col++) {
			*y = 0x80;
			y++;
			*y = 0x80;
			y++;
			*u = 0x60;
			u++;
			*v = 0xc0;
			v++;
		}
	}
	for ( ; row < (split * 3); row++) {
		for (col = 0; col < (src_width / 2); col++) {
			*y = 0xc0;
			y++;
			*y = 0xc0;
			y++;
			*u = 0xa0;
			u++;
			*v = 0x80;
			v++;
		}
	}
	for ( ; row < src_height; row++) {
		for (col = 0; col < (src_width / 2); col++) {
			*y = 0xf0;
			y++;
			*y = 0xf0;
			y++;
			*u = 0xe0;
			u++;
			*v = 0x40;
			v++;
		}
	}
}

/* output framebuffer */
static void output_fb(struct vspm_tp_private_t *priv)
{
	long fbfd;
	int ercd;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	unsigned long screensize;

	char *fbp;

	fbfd = open("/dev/fb0", O_RDWR);
	if (fbfd < 0) {
		printf("Warning: failed to open!! ercd=%d\n", fbfd);
		return;
	}

	ercd = ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
	if (ercd) {
		printf("Warning: failed to ioctl(FBIOGET_FSCREENINFO)!! ercd=%d\n", ercd);
		close(fbfd);
		return;
	}

	ercd = ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);
	if (ercd) {
		printf("Warning: failed to ioctl(FBIOGET_VSCREENINFO)!! ercd=%d\n", ercd);
		close(fbfd);
		return;
	}

	vinfo.xres = 1920;
	vinfo.yres = 1080;
	screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

	fbp = (char*)mmap(0, screensize, PROT_READ|PROT_WRITE, MAP_SHARED, fbfd, 0);
	if (fbp == NULL) {
		printf("Warning: failed to mmap!!");
		close(fbfd);
		return;
	}

	memcpy(fbp, (unsigned char*)priv->out_virt, screensize);

	ercd = ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
	if (ercd) {
		printf("Warinig: failed to ioctl(FBIOPAN_DISPLAY)!! ercd=%d\n", ercd);
	}

	ercd = close(fbfd);
	if (ercd) {
		printf("Warning: failed to close!! ercd=%d\n", ercd);
	}
}

/* callback function */
static void cb_func(
	unsigned long job_id, long result, void *user_data)
{
	struct vspm_tp_cb_info_t *cb_info =
		(struct vspm_tp_cb_info_t *)user_data;

	pthread_mutex_lock(&cb_info->mutex);
	cb_info->job_id = job_id;
	cb_info->ercd = result;
	pthread_cond_signal(&cb_info->cond);
	pthread_mutex_unlock(&cb_info->mutex);
}

/* image test */
int vsp_image_test(struct vspm_tp_private_t *priv, int scaleup)
{
	struct vspm_tp_cb_info_t *cb_info = NULL;

	struct vspm_job_t vspm_ip;
	struct vsp_start_t vsp_par;

	struct vsp_src_t src_par;
	struct vsp_alpha_unit_t alpha_par;
	struct vsp_dst_t dst_par;
	struct vsp_ctrl_t ctrl_par;
	struct vsp_uds_t uds_par;

	unsigned long job_id;
	long ercd;

	/* allocate memory */
	cb_info = malloc(sizeof(struct vspm_tp_cb_info_t));
	if (cb_info == NULL) {
		printf("Error: failed to allocate memory!!\n");
		return -1;
	}

	/* clear memory */
	memset(cb_info, 0, sizeof(struct vspm_tp_cb_info_t));
	pthread_mutex_init(&cb_info->mutex, NULL);
	pthread_cond_init(&cb_info->cond, NULL);

	/* set alpha of source */
	memset(&alpha_par, 0, sizeof(alpha_par));
	alpha_par.addr_a	= 0;
	alpha_par.stride_a	= 0;
	alpha_par.swap		= VSP_SWAP_NO;
	alpha_par.asel		= VSP_ALPHA_NUM5;
	alpha_par.aext		= VSP_AEXT_EXPAN;
	alpha_par.anum0		= 0;
	alpha_par.anum1		= 0;
	alpha_par.afix		= 0xFF;
	alpha_par.irop		= NULL;
	alpha_par.ckey		= NULL;
	alpha_par.mult		= NULL;

	/* set source */
	memset(&src_par, 0, sizeof(src_par));
	src_par.addr		= priv->in_hard;
	src_par.addr_c0		= priv->in_hard + (src_width * src_height);
	src_par.addr_c1		= priv->in_hard + (src_width * src_height) + (src_width * src_height / 2);
	src_par.stride 		= src_width;
	src_par.stride_c	= 0;
	src_par.width		= src_width;
	src_par.height		= src_height;
	src_par.width_ex	= 0;
	src_par.height_ex	= 0;
	src_par.x_offset	= 0;
	src_par.y_offset	= 0;
	src_par.format		= VSP_IN_YUV422_PLANAR;
	src_par.swap		= VSP_SWAP_B|VSP_SWAP_W|VSP_SWAP_L|VSP_SWAP_LL;
	src_par.x_position	= 0;
	src_par.y_position	= 0;
	src_par.pwd			= VSP_LAYER_PARENT;
	src_par.cipm		= VSP_CIPM_0_HOLD;
	src_par.cext		= VSP_CEXT_EXPAN;
	src_par.csc			= VSP_CSC_ON;		/* yuv -> argb */
	src_par.iturbt		= VSP_ITURBT_709;
	src_par.clrcng		= VSP_FULL_COLOR;
	src_par.vir			= VSP_NO_VIR;
	src_par.vircolor	= 0;
	src_par.clut		= NULL;
	src_par.alpha		= &alpha_par;
	if (scaleup) {
		src_par.connect = VSP_UDS_USE;
	} else {
		src_par.connect = 0;	/* wpf */
	}

	/* destination */
	memset(&dst_par, 0, sizeof(dst_par));
	dst_par.addr		= priv->out_hard;
	dst_par.addr_c0		= 0;
	dst_par.addr_c1		= 0;
	dst_par.stride		= dst_width * 4;
	dst_par.stride_c	= 0;
	dst_par.format		= VSP_OUT_PRGB8888;
	dst_par.csc			= VSP_CSC_OFF;
	if (scaleup) {
		dst_par.width	= dst_width;
		dst_par.height	= dst_height;
	} else {
		dst_par.width	= src_width;
		dst_par.height	= src_height;
	}
	dst_par.x_offset	= 0;
	dst_par.y_offset	= 0;
	dst_par.swap		= VSP_SWAP_L|VSP_SWAP_LL;
	dst_par.pxa			= VSP_PAD_P;
	dst_par.pad			= 0;
	dst_par.x_coffset	= 0;
	dst_par.y_coffset	= 0;
	dst_par.iturbt		= VSP_ITURBT_709;
	dst_par.clrcng		= VSP_ITU_COLOR;
	dst_par.cbrm		= VSP_CSC_ROUND_DOWN;
	dst_par.abrm		= VSP_CONVERSION_ROUNDDOWN;
	dst_par.athres		= 0;
	dst_par.clmd		= VSP_CLMD_NO;
	dst_par.dith		= VSP_DITH_OFF;
	dst_par.rotation	= VSP_ROT_OFF;
	dst_par.fcp			= NULL;

	memset(&ctrl_par, 0x00, sizeof(ctrl_par));
	if (scaleup) {
		/* uds */
		memset(&uds_par, 0, sizeof(uds_par));
		uds_par.amd			= VSP_AMD;
		uds_par.clip		= VSP_CLIP_OFF;
		uds_par.alpha		= VSP_ALPHA_ON;
		uds_par.complement	= VSP_COMPLEMENT_BIL;
		uds_par.athres0		= 0;
		uds_par.athres1		= 0;
		uds_par.anum0		= 0;
		uds_par.anum1		= 0;
		uds_par.anum2		= 0;
		uds_par.x_ratio		= src_width * 4096 / dst_width;
		uds_par.y_ratio		= src_height * 4096 / dst_height;
		uds_par.connect		= 0;	/* wpf */

		/* set module */
		ctrl_par.uds		= &uds_par;
	}

	/* set vsp */
	memset(&vsp_par, 0x00, sizeof(vsp_par));
	vsp_par.rpf_num		= 1;
	if (scaleup) {
		vsp_par.use_module	= VSP_UDS_USE;
	}
	vsp_par.src_par[0]	= &src_par;
	vsp_par.src_par[1]	= NULL;
	vsp_par.src_par[2]	= NULL;
	vsp_par.src_par[3]	= NULL;
	vsp_par.src_par[4]	= NULL;
	vsp_par.dst_par		= &dst_par;
	vsp_par.ctrl_par	= &ctrl_par;
	/* this parameter is necessary for scaling, super resolution and rotation */
	if (scaleup) {
		vsp_par.dl_par.hard_addr = priv->dl_hard;
		vsp_par.dl_par.virt_addr = priv->dl_virt;
		vsp_par.dl_par.tbl_num	 = tbl_num;
	}

	/* set vspm */
	memset(&vspm_ip, 0, sizeof(struct vspm_job_t));
	vspm_ip.type = VSPM_TYPE_VSP_AUTO;
	vspm_ip.par.vsp = &vsp_par;

	/* entry */
	ercd = vspm_entry_job(priv->handle, &job_id, 126, &vspm_ip, (void *)cb_info, cb_func);
	if (ercd != R_VSPM_OK) {
		printf("Error: failed to vspm_entry_job() ercd=%d\n", ercd);
		pthread_mutex_destroy(&cb_info->mutex);
		free(cb_info);
		return -1;
	}

	/* wait callback */
	pthread_mutex_lock(&cb_info->mutex);
	pthread_cond_wait(&cb_info->cond, &cb_info->mutex);
	pthread_mutex_unlock(&cb_info->mutex);

	/* check callback information */
	if ((cb_info->ercd != 0) || (cb_info->job_id != job_id)) {
		printf("Error: failed to incorrect callback information!!\n");
		pthread_mutex_destroy(&cb_info->mutex);
		free(cb_info);
		return -1;
	}

	pthread_mutex_destroy(&cb_info->mutex);
	free(cb_info);
	return 0;
}

/* main function */
int main(int argc, char *argv[])
{
	struct vspm_tp_private_t *priv = NULL;
	struct vspm_init_t init_par;
	long ercd;
	struct timeval pre_time, now_time;
	unsigned int time;

	/* allocate private memory */
	priv = malloc(sizeof(struct vspm_tp_private_t));
	if (priv == NULL) {
		printf("Error: failed to allocate memory!!\n");
		return -1;
	}
	memset(priv, 0, sizeof(struct vspm_tp_private_t));

	/* allocate memory from memory manager */
	ercd = (long)allocate_memory(priv);
	if (ercd != 0) {
		goto err_exit0;
	}

	/* create source image */
	create_source_image(priv);

	/* init vsp manager */
	memset(&init_par, 0, sizeof(struct vspm_init_t));
	init_par.use_ch = VSPM_EMPTY_CH;
	init_par.mode = VSPM_MODE_MUTUAL;
	init_par.type = VSPM_TYPE_VSP_AUTO;

	ercd = vspm_init_driver(&priv->handle, &init_par);
	if (ercd != R_VSPM_OK) {
		printf("Error: failed to vspm_init_driver() ercd=%d\n", ercd);
		goto err_exit1;
	}

	/* no scale up */
	ercd = (long)vsp_image_test(priv, 0);
	if (ercd != 0) {
		goto err_exit2;
	}
#if (FB_ENABLE == 1)
	output_fb(priv);
	sleep(3);
#endif

	gettimeofday(&pre_time, NULL);

	/* scale up */
	ercd = (long)vsp_image_test(priv, 1);
	if (ercd != 0) {
		goto err_exit2;
	}

	gettimeofday(&now_time, NULL);
	time  = (now_time.tv_sec - pre_time.tv_sec) * 1000000;
	time += now_time.tv_usec;
	time -= pre_time.tv_usec;

	ercd = vspm_quit_driver(priv->handle);
	if (ercd != R_VSPM_OK) {
		printf("Error: failed to vspm_quit_driver() ercd=%d\n", ercd);
		goto err_exit1;
	}

#if (FB_ENABLE == 1)
	output_fb(priv);
#endif

	/* release memory */
	release_memory(priv);
	free(priv);

	printf("PASS: up scale time = %d [us]\n", time);
	return 0;

err_exit2:
	(void)vspm_quit_driver(priv->handle);

err_exit1:
	release_memory(priv);

err_exit0:
	free(priv);

	printf("FAIL\n", 0);
	return -1;
}
