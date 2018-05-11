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

struct vspm_tp_private_t {
	void *in_virt[2];
	unsigned int in_hard[2];
	MMNGR_ID in_fd[2];

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
	if (priv->in_fd[1] != 0)
		(void)mmngr_free_in_user_ext(priv->in_fd[1]);
	if (priv->in_fd[0] != 0)
		(void)mmngr_free_in_user_ext(priv->in_fd[0]);

	return 0;
}

/* allocate memory from memory manager */
static int allocate_memory(struct vspm_tp_private_t *priv)
{
	struct MM_FUNC mem;
	unsigned int conf;
	int ercd;

	/* input buffer (ARGB8888) */
	ercd = mmngr_alloc_in_user_ext(
		&priv->in_fd[0], 960*960*4, &priv->in_hard[0], &priv->in_virt[0], MMNGR_VA_SUPPORT, NULL);
	if (ercd != R_MM_OK) {
		printf("Error: failed to allocate memory for input!! ercd=%d\n", ercd);
		return -1;
	}

	/* input buffer (ARGB8888) */
	ercd = mmngr_alloc_in_user_ext(
		&priv->in_fd[1], 960*960*4, &priv->in_hard[1], &priv->in_virt[1], MMNGR_VA_SUPPORT, NULL);
	if (ercd != R_MM_OK) {
		printf("Error: failed to allocate memory for input!! ercd=%d\n", ercd);
		return -1;
	}

	/* output buffer (ARGB8888) */
	ercd = mmngr_alloc_in_user_ext(
		&priv->out_fd, 1920*1080*4, &priv->out_hard, &priv->out_virt, MMNGR_VA_SUPPORT, NULL);
	if (ercd != R_MM_OK) {
		printf("Error: failed to allocate memory for output!! ercd=%d\n", ercd);
		(void)release_memory(priv);
		return -1;
	}
	memset(priv->out_virt, 0, 1920*1080*4);

	/* display list */
	ercd = mmngr_alloc_in_user_ext(
		&priv->dl_fd, (192+64*4)*8, &priv->dl_hard, &priv->dl_virt, MMNGR_VA_SUPPORT, NULL);
	if (ercd != R_MM_OK) {
		printf("Error: failed to allocate memory for DL!! ercd=%d\n", ercd);
		(void)release_memory(priv);
		return -1;
	}
	memset((void *)priv->dl_virt, 0, (192+64*4)*8);

	return 0;
}

/* create source image */
static void create_source_image(struct vspm_tp_private_t *priv)
{
	unsigned int *canvas;
	unsigned int *canvas2;
	int row, col;
	unsigned int red, green, blue;

	red = 0x00FF0000;
	green = 0x0000FF00;
	blue = 0x000000FF;

	/* draw back color */
	canvas = (unsigned int *)priv->in_virt[0];
	canvas2 = (unsigned int *)priv->in_virt[1];

	for (row = 0; row < 160; row++) {
		for (col = 0; col < 960; col++) {
			*canvas++ = red;
			*canvas2++ = green;
		}
	}
	for ( ; row < 160*2; row++) {
		for (col = 0; col < 960; col++) {
			*canvas++ = green;
			*canvas2++ = blue;
		}
	}
	for ( ; row < 160*3; row++) {
		for (col = 0; col < 960; col++) {
			*canvas++ = blue;
			*canvas2++ = red;
		}
	}
	for ( ; row < 160*4; row++) {
		for (col = 0; col < 960; col++) {
			*canvas++ = red;
			*canvas2++ = green;
		}
	}
	for ( ; row < 160*5; row++) {
		for (col = 0; col < 960; col++) {
			*canvas++ = green;
			*canvas2++ = blue;
		}
	}
	for ( ; row < 160*6; row++) {
		for (col = 0; col < 960; col++) {
			*canvas++ = blue;
			*canvas2++ = red;
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

/* blending test */
int vsp_blend_test(struct vspm_tp_private_t *priv, int keying)
{
	struct vspm_tp_cb_info_t *cb_info = NULL;

	struct vspm_job_t vspm_ip;
	struct vsp_start_t vsp_par;

	struct vsp_src_t src_par[2];
	struct vsp_alpha_unit_t alpha_par;
	struct vsp_dst_t dst_par;
	struct vsp_ctrl_t ctrl_par;

	struct vsp_bru_t bru_par;
	struct vsp_bld_ctrl_t bru_ctrl_par;
	struct vsp_bld_vir_t bru_vir_par;

	unsigned long job_id;
	long ercd;

	unsigned char num = 0;

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

	memset(&vsp_par, 0, sizeof(struct vsp_start_t));

	/* set color-keying of source */
	struct vsp_ckey_unit_t pckey;
	pckey.mode = VSP_CKEY_TRANS_COLOR1;
	pckey.color1 = 0x0000FF00;  /* green */
	pckey.color2 = 0x00FFFF00;

	/* set alpha of source */
	memset(&alpha_par, 0, sizeof(alpha_par));
	alpha_par.addr_a	= 0;
	alpha_par.stride_a	= 0;
	alpha_par.swap		= VSP_SWAP_L|VSP_SWAP_LL;
	alpha_par.asel		= VSP_ALPHA_NUM5;
	alpha_par.aext		= VSP_AEXT_EXPAN;
	alpha_par.anum0		= 0;
	alpha_par.anum1		= 0;
	alpha_par.afix		= 0xFF;
	alpha_par.irop		= NULL;
	if (keying) {
		alpha_par.ckey	= &pckey;
	}
	alpha_par.mult		= NULL;

	/* set source */
	memset(src_par, 0, sizeof(src_par));

	src_par[num].addr		= priv->in_hard[0];
	src_par[num].addr_c0	= 0;
	src_par[num].addr_c1	= 0;
	src_par[num].stride 	= 960 * 4;
	src_par[num].stride_c	= 0;
	src_par[num].format		= VSP_IN_ARGB8888;
	src_par[num].csc		= VSP_CSC_OFF;

	src_par[num].width		= 960;
	src_par[num].height		= 960;
	src_par[num].width_ex	= 0;
	src_par[num].height_ex	= 0;
	src_par[num].x_offset	= 0;
	src_par[num].y_offset	= 0;
	src_par[num].swap		= VSP_SWAP_L|VSP_SWAP_LL;
	src_par[num].x_position	= 0;
	src_par[num].y_position	= 60;
	src_par[num].pwd		= VSP_LAYER_CHILD;
	src_par[num].cipm		= VSP_CIPM_0_HOLD;
	src_par[num].cext		= VSP_CEXT_EXPAN;
	src_par[num].iturbt		= VSP_ITURBT_709;
	src_par[num].clrcng		= VSP_FULL_COLOR;
	src_par[num].vir		= VSP_NO_VIR;
	src_par[num].vircolor	= 0;
	src_par[num].clut		= NULL;
	src_par[num].alpha		= &alpha_par;
	src_par[num].connect 	= VSP_BRU_USE;	/* connect BRU */

	vsp_par.src_par[num]	= &src_par[num];

	num++;

	src_par[num].addr		= priv->in_hard[1];
	src_par[num].addr_c0	= 0;
	src_par[num].addr_c1	= 0;
	src_par[num].stride 	= 960 * 4;
	src_par[num].stride_c	= 0;
	src_par[num].format		= VSP_IN_ARGB8888;
	src_par[num].csc		= VSP_CSC_OFF;

	src_par[num].width		= 960;
	src_par[num].height		= 960;
	src_par[num].width_ex	= 0;
	src_par[num].height_ex	= 0;
	src_par[num].x_offset	= 0;
	src_par[num].y_offset	= 0;
	src_par[num].swap		= VSP_SWAP_L|VSP_SWAP_LL;
	src_par[num].x_position	= 960;
	src_par[num].y_position	= 60;
	src_par[num].pwd		= VSP_LAYER_CHILD;
	src_par[num].cipm		= VSP_CIPM_0_HOLD;
	src_par[num].cext		= VSP_CEXT_EXPAN;
	src_par[num].iturbt		= VSP_ITURBT_709;
	src_par[num].clrcng		= VSP_FULL_COLOR;
	src_par[num].vir		= VSP_NO_VIR;
	src_par[num].vircolor	= 0;
	src_par[num].clut		= NULL;
	src_par[num].alpha		= &alpha_par;
	src_par[num].connect 	= VSP_BRU_USE;	/* connect BRU */

	vsp_par.src_par[num]	= &src_par[num];

	num++;

	/* set destination */
	memset(&dst_par, 0, sizeof(dst_par));
	dst_par.addr		= priv->out_hard;
	dst_par.addr_c0		= 0;
	dst_par.addr_c1		= 0;
	dst_par.stride		= 1920 * 4;
	dst_par.stride_c	= 0;
	dst_par.width		= 1920;
	dst_par.height		= 1080;
	dst_par.x_offset	= 0;
	dst_par.y_offset	= 0;
	dst_par.format		= VSP_OUT_PRGB8888;
	dst_par.swap		= VSP_SWAP_L|VSP_SWAP_LL;
	dst_par.pxa			= VSP_PAD_P;
	dst_par.pad			= 0;
	dst_par.x_coffset	= 0;
	dst_par.y_coffset	= 0;
	dst_par.csc			= VSP_CSC_OFF;
	dst_par.iturbt		= VSP_ITURBT_709;
	dst_par.clrcng		= VSP_ITU_COLOR;
	dst_par.cbrm		= VSP_CSC_ROUND_DOWN;
	dst_par.abrm		= VSP_CONVERSION_ROUNDDOWN;
	dst_par.athres		= 0;
	dst_par.clmd		= VSP_CLMD_NO;
	dst_par.dith		= VSP_DITH_OFF;
	dst_par.rotation	= VSP_ROT_OFF;
	dst_par.fcp			= NULL;

	/* set module */
	memset(&bru_ctrl_par, 0, sizeof(struct vsp_bld_ctrl_t));
	bru_ctrl_par.rbc			= VSP_RBC_BLEND;
	bru_ctrl_par.crop			= 0;
	bru_ctrl_par.arop			= 0;
	bru_ctrl_par.blend_formula	= VSP_FORM_BLEND0;
	bru_ctrl_par.blend_coefx	= VSP_COEFFICIENT_BLENDX4;
	bru_ctrl_par.blend_coefy	= VSP_COEFFICIENT_BLENDY3;
	bru_ctrl_par.aformula		= VSP_FORM_ALPHA0;
	bru_ctrl_par.acoefx			= VSP_COEFFICIENT_ALPHAX5;
	bru_ctrl_par.acoefy			= VSP_COEFFICIENT_ALPHAY5;
	bru_ctrl_par.acoefx_fix		= 1;
	bru_ctrl_par.acoefy_fix		= 0;

	memset(&bru_vir_par, 0, sizeof(struct vsp_bld_vir_t));
	bru_vir_par.width		= 1920;
	bru_vir_par.height		= 1080;
	bru_vir_par.x_position	= 0;
	bru_vir_par.y_position	= 0;
	bru_vir_par.pwd			= VSP_LAYER_PARENT;
	bru_vir_par.color		= 0x00808080;

	memset(&bru_par, 0, sizeof(struct vsp_bru_t));
	bru_par.lay_order		= (VSP_LAY_VIRTUAL | (VSP_LAY_1 << 4) | (VSP_LAY_2 << 8));
	bru_par.blend_virtual	= &bru_vir_par;
	bru_par.blend_unit_a	= &bru_ctrl_par;
	bru_par.blend_unit_b	= &bru_ctrl_par;
	bru_par.connect			= 0;	/* connect WPF */

	memset(&ctrl_par, 0, sizeof(struct vsp_ctrl_t));
	ctrl_par.bru = &bru_par;

	/* set vsp */
	vsp_par.rpf_num		= num;
	vsp_par.use_module	= VSP_BRU_USE;
	vsp_par.dst_par		= &dst_par;
	vsp_par.ctrl_par	= &ctrl_par;
	/* this parameter is necessary for scaling, super resolution and rotation */
	/*
	vsp_par.dl_par.hard_addr = priv->dl_hard;
	vsp_par.dl_par.virt_addr = priv->dl_virt;
	vsp_par.dl_par.tbl_num	 = 192+64*4;
	*/

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

	/* blend processing ( not keying ) */
	ercd = (long)vsp_blend_test(priv, 0);
	if (ercd != 0) {
		goto err_exit2;
	}

#if (FB_ENABLE == 1)
	output_fb(priv);
	sleep(3);
#endif

	gettimeofday(&pre_time, NULL);

	/* blend processing ( color keying ) */
	ercd = (long)vsp_blend_test(priv, 1);
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

	printf("PASS: blend time = %d [us]\n", time);
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
