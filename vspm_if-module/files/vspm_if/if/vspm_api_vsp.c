/* 
 * Copyright (c) 2015-2016 Renesas Electronics Corporation
 * Released under the MIT license
 * http://opensource.org/licenses/mit-license.php 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "vspm_public.h"
#include "mmngr_user_public.h"

struct vspm_if_vsp_handle_t {
	void *handle;
	pthread_mutex_t mutex;
	struct vspm_if_vsp_cb_t *cb_list;
};

struct vspm_if_vsp_par_t {
	struct vspm_job_t job_par;
	struct vsp_start_t start_par;
	struct vspm_if_vsp_src_par_t {
		struct vsp_src_t src_par;
		struct vsp_dl_t clut;	/* clut.virt_addr */
		struct vsp_alpha_unit_t alpha;
		struct vsp_irop_unit_t irop;
		struct vsp_ckey_unit_t ckey;
	} src[4];
	struct vsp_dst_t dst_par;
	struct vspm_if_vsp_ctrl_par_t {
		struct vsp_ctrl_t ctrl;
		struct vsp_sru_t sru;
		struct vsp_uds_t uds;
		struct vspm_if_vsp_lut_par_t {
			struct vsp_lut_t lut;
			MMNGR_ID fd;
		} lut;
		struct vspm_if_vsp_clu_par_t {
			struct vsp_clu_t clu;
			MMNGR_ID fd;
		} clu;
		struct vsp_hst_t hst;
		struct vsp_hsi_t hsi;
		struct vspm_if_vsp_bru_par_t {
			struct vsp_bru_t bru;
			struct vsp_bld_dither_t dith[4];
			struct vsp_bld_vir_t vir;
			struct vsp_bld_ctrl_t ctrl[4];
			struct vsp_bld_rop_t rop;
		} bru;
		struct vspm_if_vsp_hgo_par_t {
			struct vsp_hgo_t hgo;	/* hgo.virt_addr */
			void *old_addr;
		} hgo;
		struct vspm_if_vsp_hgt_par_t {
			struct vsp_hgt_t hgt;	/* hgt.virt_addr */
			void *old_addr;
		} hgt;
	} ctrl;
};

struct vspm_if_vsp_cb_t {
	struct vspm_if_vsp_handle_t *parent_hdl;
	struct vspm_if_vsp_par_t vsp_par;
	unsigned long user_data;
	void (*cb_func)
		(unsigned long job_id, long result, unsigned long user_data);
	struct vspm_if_vsp_cb_t *next;
};


static void vspm_if_add_vsp_cb_info(
	struct vspm_if_vsp_handle_t *hdl, struct vspm_if_vsp_cb_t *set_cb)
{
	pthread_mutex_lock(&hdl->mutex);

	/* regist parent handle */
	set_cb->parent_hdl = hdl;

	/* regist callback list */
	set_cb->next = hdl->cb_list;
	hdl->cb_list = set_cb;

	pthread_mutex_unlock(&hdl->mutex);
}


static long vspm_if_del_vsp_cb_info(struct vspm_if_vsp_cb_t *target_cb)
{
	struct vspm_if_vsp_handle_t *hdl = target_cb->parent_hdl;
	struct vspm_if_vsp_cb_t *cb;
	struct vspm_if_vsp_cb_t *pre_cb;

	long ercd = R_VSPM_NG;

	pthread_mutex_lock(&hdl->mutex);

	cb = hdl->cb_list;
	pre_cb = NULL;

	while (cb != NULL) {
		if (cb == target_cb) {
			if (pre_cb == NULL) {
				hdl->cb_list = cb->next;
			} else {
				pre_cb->next = cb->next;
			}
			ercd = R_VSPM_OK;
			break;
		}

		pre_cb = cb;
		cb = cb->next;
	}

	pthread_mutex_unlock(&hdl->mutex);

	return ercd;
}


static void vspm_if_expand_vsp_lut_tbl(
	unsigned int *dst,
	unsigned int src_addr,
	unsigned int *src_data,
	unsigned short num)
{
	unsigned short i;

	for (i = 0; i < num; i++) {
		*dst++ = src_addr;
		*dst++ = *src_data++;
		src_addr += 4;
	}
}


static void vspm_if_expand_vsp_clu_tbl(
	unsigned int *dst,
	unsigned int *src_addr,
	unsigned int *src_data,
	unsigned short num)
{
	unsigned short i;

	for (i = 0; i < num; i++) {
		*dst++ = 0x00007400;
		*dst++ = *src_addr++;
		*dst++ = 0x00007404;
		*dst++ = *src_data++;
	}
}


static void vspm_if_set_vsp_src_param(
	struct vspm_if_vsp_src_par_t *par, T_VSP_IN *old_src_par)
{
	struct vsp_src_t *src_par = &par->src_par;

	unsigned short tbl_num;
	unsigned int *data = NULL;

	unsigned long tmp;

	tmp = (unsigned long)old_src_par->addr;
	src_par->addr = (unsigned int)(tmp & 0xffffffff);
	tmp = (unsigned long)old_src_par->addr_c0;
	src_par->addr_c0 = (unsigned int)(tmp & 0xffffffff);
	tmp = (unsigned long)old_src_par->addr_c1;
	src_par->addr_c1 = (unsigned int)(tmp & 0xffffffff);
	src_par->stride = old_src_par->stride;
	src_par->stride_c = old_src_par->stride_c;
	src_par->width = old_src_par->width;
	src_par->height = old_src_par->height;
	src_par->width_ex = old_src_par->width_ex;
	src_par->height_ex = old_src_par->height_ex;
	src_par->x_offset = old_src_par->x_offset;
	src_par->y_offset = old_src_par->y_offset;
	src_par->format = old_src_par->format;
	src_par->swap = old_src_par->swap;
	src_par->x_position = old_src_par->x_position;
	src_par->y_position = old_src_par->y_position;
	src_par->pwd = old_src_par->pwd;
	src_par->cipm = old_src_par->cipm;
	src_par->cext = old_src_par->cext;
	src_par->csc = old_src_par->csc;
	src_par->iturbt = old_src_par->iturbt;
	src_par->clrcng = old_src_par->clrcng;
	src_par->vir = old_src_par->vir;
	src_par->vircolor = old_src_par->vircolor;

	if (old_src_par->osd_lut != NULL) {
		if (old_src_par->osd_lut->clut != NULL) {
			tbl_num = (unsigned short)old_src_par->osd_lut->size;
			if (old_src_par->osd_lut->size > 256) {
				tbl_num = 256;
			}

			data = malloc(tbl_num * 8);
			if (data != NULL) {
				vspm_if_expand_vsp_lut_tbl(
					data,
					0,
					(unsigned int *)old_src_par->osd_lut->clut,
					tbl_num);
			}
		}

		par->clut.hard_addr = 0;
		par->clut.virt_addr = (void *)data;
		par->clut.tbl_num = (unsigned short)old_src_par->osd_lut->size;

		src_par->clut = &par->clut;
	} else {
		src_par->clut = NULL;
	}

	if (old_src_par->alpha_blend != NULL) {
		tmp = (unsigned long)old_src_par->alpha_blend->addr_a;
		par->alpha.addr_a = (unsigned int)(tmp & 0xffffffff);
		par->alpha.stride_a = old_src_par->alpha_blend->astride;
		par->alpha.swap = old_src_par->alpha_blend->aswap;
		par->alpha.asel = old_src_par->alpha_blend->asel;
		par->alpha.aext = old_src_par->alpha_blend->aext;
		par->alpha.anum0 = old_src_par->alpha_blend->anum0;
		par->alpha.anum1 = old_src_par->alpha_blend->anum1;
		par->alpha.afix = old_src_par->alpha_blend->afix;

		par->irop.op_mode = old_src_par->alpha_blend->irop;
		par->irop.ref_sel = old_src_par->alpha_blend->msken;
		par->irop.bit_sel = old_src_par->alpha_blend->bsel;
		par->irop.comp_color = old_src_par->alpha_blend->mgcolor;
		par->irop.irop_color0 = old_src_par->alpha_blend->mscolor0;
		par->irop.irop_color1 = old_src_par->alpha_blend->mscolor1;

		if (old_src_par->alpha_blend->alphan == VSP_ALPHA_AL1) {
			par->ckey.mode = VSP_CKEY_TRANS_COLOR1;
			par->ckey.color1 = old_src_par->alpha_blend->alpha1;
			par->ckey.color2 = 0;
		} else if (old_src_par->alpha_blend->alphan == VSP_ALPHA_AL2) {
			par->ckey.mode = VSP_CKEY_TRANS_COLOR2;
			par->ckey.color1 = old_src_par->alpha_blend->alpha1;
			par->ckey.color2 = old_src_par->alpha_blend->alpha2;
		} else {
			par->ckey.mode = VSP_CKEY_THROUGH;
			par->ckey.color1 = 0;
			par->ckey.color2 = 0;
		}

		par->alpha.irop = &par->irop;
		par->alpha.ckey = &par->ckey;
		par->alpha.mult = NULL;
		src_par->alpha = &par->alpha;
	}

	if ((old_src_par->clrcnv != NULL) &&
		(par->ckey.mode == VSP_CKEY_THROUGH)) {
		par->ckey.mode = VSP_CKEY_MATCHED_COLOR;
		par->ckey.color1 = old_src_par->clrcnv->color1;
		par->ckey.color2 = old_src_par->clrcnv->color2;

		par->alpha.ckey = &par->ckey;
		src_par->alpha = &par->alpha;
	}

	src_par->connect = old_src_par->connect;
}


static void vspm_if_set_vsp_dst_param(
	struct vsp_dst_t *dst, T_VSP_OUT *old_dst)
{
	unsigned long tmp;

	tmp = (unsigned long)old_dst->addr;
	dst->addr = (unsigned int)(tmp & 0xffffffff);
	tmp = (unsigned long)old_dst->addr_c0;
	dst->addr_c0 = (unsigned int)(tmp & 0xffffffff);;
	tmp = (unsigned long)old_dst->addr_c1;
	dst->addr_c1 = (unsigned int)(tmp & 0xffffffff);;
	dst->stride = old_dst->stride;
	dst->stride_c = old_dst->stride_c;
	dst->width = old_dst->width;
	dst->height = old_dst->height;
	dst->x_offset = old_dst->x_offset;
	dst->y_offset = old_dst->y_offset;
	dst->format = old_dst->format;
	dst->swap = old_dst->swap;
	dst->pxa = old_dst->pxa;
	dst->pad = old_dst->pad;
	dst->x_coffset = old_dst->x_coffset;
	dst->y_coffset = old_dst->y_coffset;
	dst->csc = old_dst->csc;
	dst->iturbt = old_dst->iturbt;
	dst->clrcng = old_dst->clrcng;
	dst->cbrm = old_dst->cbrm;
	dst->abrm = old_dst->abrm;
	dst->athres = old_dst->athres;
	dst->clmd = old_dst->clmd;
	if (old_dst->dith == VSP_DITHER) {
		dst->dith = VSP_DITH_COLOR_REDUCTION;
	} else {
		dst->dith = VSP_DITH_OFF;
	}
	dst->rotation = VSP_ROT_OFF;
	dst->fcp = NULL;
}


static void vspm_if_set_vsp_sru_param(
	struct vsp_sru_t *sru, T_VSP_SRU *old_sru)
{
	sru->mode = old_sru->mode;
	sru->param = old_sru->param;
	sru->enscl = old_sru->enscl;
	sru->fxa = old_sru->fxa;
	sru->connect = old_sru->connect;
}


static void vspm_if_set_vsp_uds_param(
	struct vsp_uds_t *uds, T_VSP_UDS *old_uds)
{
	uds->amd = old_uds->amd;
	uds->clip = old_uds->clip;
	uds->alpha = old_uds->alpha;
	uds->complement = old_uds->complement;
	uds->athres0 = old_uds->athres0;
	uds->athres1 = old_uds->athres1;
	uds->anum0 = old_uds->anum0;
	uds->anum1 = old_uds->anum1;
	uds->anum2 = old_uds->anum2;
	uds->x_ratio = old_uds->x_ratio;
	uds->y_ratio = old_uds->y_ratio;
	uds->connect = old_uds->connect;
}


static void vspm_if_set_vsp_lut_param(
	struct vspm_if_vsp_lut_par_t *par, T_VSP_LUT *old_lut)
{
	struct vsp_lut_t *lut = &par->lut;
	unsigned short tbl_num;

	unsigned int hard_addr = 0;
	void *virt_addr;
	int ercd;

	if (old_lut->lut != NULL) {
		tbl_num = (unsigned short)old_lut->size;
		if (old_lut->size > 256) {
			tbl_num = 256;
		}

		ercd = mmngr_alloc_in_user_ext(
			&par->fd, tbl_num * 8, &hard_addr, &virt_addr, MMNGR_VA_SUPPORT, NULL);
		if (ercd == R_MM_OK) {
			vspm_if_expand_vsp_lut_tbl(
				(unsigned int *)virt_addr,
				0x00007000,
				(unsigned int *)old_lut->lut,
				tbl_num);
		}
	}

	lut->lut.hard_addr = hard_addr;
	lut->lut.virt_addr = NULL;
	lut->lut.tbl_num = (unsigned short)old_lut->size;
	lut->fxa = old_lut->fxa;
	lut->connect = old_lut->connect;
}

static void vspm_if_set_vsp_clu_param(
	struct vspm_if_vsp_clu_par_t *par, T_VSP_CLU *old_clu)
{
	struct vsp_clu_t *clu = &par->clu;
	unsigned short tbl_num;

	unsigned int hard_addr = 0;
	void *virt_addr;
	int ercd;

	tbl_num = (unsigned short)old_clu->size;
	if (tbl_num > 4913) {
		tbl_num = 4913;
	}

	if ((old_clu->mode == VSP_CLU_MODE_3D) ||
		(old_clu->mode == VSP_CLU_MODE_2D)) {
		/* normal mode */
		if ((old_clu->clu_addr != NULL) &&
			(old_clu->clu_data != NULL)) {

			ercd = mmngr_alloc_in_user_ext(
				&par->fd,
				tbl_num * 16,
				&hard_addr,
				&virt_addr,
				MMNGR_VA_SUPPORT,
				NULL);
			if (ercd == R_MM_OK) {
				vspm_if_expand_vsp_clu_tbl(
					(unsigned int *)virt_addr,
					(unsigned int *)old_clu->clu_addr,
					(unsigned int *)old_clu->clu_data,
					tbl_num);
			}

			clu->clu.tbl_num = tbl_num * 2;
		}
	} else {
		/* auto address increment mode */
		if (old_clu->clu_data != NULL) {
			ercd = mmngr_alloc_in_user_ext(
				&par->fd,
				tbl_num * 8,
				&hard_addr,
				&virt_addr,
				MMNGR_VA_SUPPORT,
				NULL);
			if (ercd == R_MM_OK) {
				vspm_if_expand_vsp_lut_tbl(
					(unsigned int *)virt_addr,
					0x00007404,
					(unsigned int *)old_clu->clu_data,
					tbl_num);
			}

			clu->clu.tbl_num = tbl_num;
		}
	}

	clu->mode = old_clu->mode;
	clu->clu.hard_addr = hard_addr;
	clu->clu.virt_addr = NULL;
	clu->fxa = old_clu->fxa;
	clu->connect = old_clu->connect;
}

static void vspm_if_set_vsp_hst_param(
	struct vsp_hst_t *hst, T_VSP_HST *old_hst)
{
	hst->fxa = old_hst->fxa;
	hst->connect = old_hst->connect;
}


static void vspm_if_set_vsp_hsi_param(
	struct vsp_hsi_t *hsi, T_VSP_HSI *old_hsi)
{
	hsi->fxa = old_hsi->fxa;
	hsi->connect = old_hsi->connect;
}


static void vspm_if_set_vsp_bru_vir_param(
	struct vsp_bld_vir_t *vir, T_VSP_BLEND_VIRTUAL *old_vir)
{
	vir->width = old_vir->width;
	vir->height = old_vir->height;
	vir->x_position = old_vir->x_position;
	vir->y_position = old_vir->y_position;
	vir->pwd = old_vir->pwd;
	vir->color = old_vir->color;
}


static void vspm_if_set_vsp_bru_ctrl_param(
	struct vsp_bld_ctrl_t *ctrl, T_VSP_BLEND_CONTROL *old_ctrl)
{
	ctrl->rbc = old_ctrl->rbc;
	ctrl->crop = old_ctrl->crop;
	ctrl->arop = old_ctrl->arop;
	ctrl->blend_formula = old_ctrl->blend_formula;
	ctrl->blend_coefx = old_ctrl->blend_coefx;
	ctrl->blend_coefy = old_ctrl->blend_coefy;
	ctrl->aformula = old_ctrl->aformula;
	ctrl->acoefx = old_ctrl->acoefx;
	ctrl->acoefy = old_ctrl->acoefy;
	ctrl->acoefx_fix = old_ctrl->acoefx_fix;
	ctrl->acoefy_fix = old_ctrl->acoefy_fix;
}


static void vspm_if_set_vsp_bru_rop_param(
	struct vsp_bld_rop_t *rop, T_VSP_BLEND_ROP *old_rop)
{
	rop->crop = old_rop->crop;
	rop->arop = old_rop->arop;
}


static void vspm_if_set_vsp_bru_param(
	struct vspm_if_vsp_bru_par_t *par, T_VSP_BRU *old_bru)
{
	struct vsp_bru_t *bru = &par->bru;
	int i;

	bru->lay_order = old_bru->lay_order;
	bru->adiv = old_bru->adiv;

	for (i = 0; i < 4; i++) {
		if (old_bru->qnt[i] == VSP_QNT_ON) {
			par->dith[i].mode = VSP_DITH_COLOR_REDUCTION;
			par->dith[i].bpp = old_bru->dith[i];

			bru->dither_unit[i] = &par->dith[i];
		} else {
			bru->dither_unit[i] = NULL;
		}
	}

	if (old_bru->blend_virtual != NULL) {
		vspm_if_set_vsp_bru_vir_param(
			&par->vir, old_bru->blend_virtual);
		bru->blend_virtual = &par->vir;
	}

	if (old_bru->blend_control_a != NULL) {
		vspm_if_set_vsp_bru_ctrl_param(
			&par->ctrl[0], old_bru->blend_control_a);
		bru->blend_unit_a = &par->ctrl[0];
	}

	if (old_bru->blend_control_b != NULL) {
		vspm_if_set_vsp_bru_ctrl_param(
			&par->ctrl[1], old_bru->blend_control_b);
		bru->blend_unit_b = &par->ctrl[1];
	}

	if (old_bru->blend_control_c != NULL) {
		vspm_if_set_vsp_bru_ctrl_param(
			&par->ctrl[2], old_bru->blend_control_c);
		bru->blend_unit_c = &par->ctrl[2];
	}

	if (old_bru->blend_control_d != NULL) {
		vspm_if_set_vsp_bru_ctrl_param(
			&par->ctrl[3], old_bru->blend_control_d);
		bru->blend_unit_d = &par->ctrl[3];
	}

	if (old_bru->blend_rop != NULL) {
		vspm_if_set_vsp_bru_rop_param(
			&par->rop, old_bru->blend_rop);
		bru->rop_unit = &par->rop;
	}

	bru->connect = old_bru->connect;
}


static void vspm_if_set_vsp_hgo_param(
	struct vspm_if_vsp_hgo_par_t *par, T_VSP_HGO *old_hgo)
{
	struct vsp_hgo_t *hgo = &par->hgo;
	unsigned long *addr;

	addr = malloc(1088);
	hgo->hard_addr = 0;
	hgo->virt_addr = (void *)addr;
	hgo->width = old_hgo->width;
	hgo->height = old_hgo->height;
	hgo->x_offset = old_hgo->x_offset;
	hgo->y_offset = old_hgo->y_offset;
	hgo->binary_mode = old_hgo->binary_mode;
	hgo->maxrgb_mode = old_hgo->maxrgb_mode;
	hgo->step_mode = VSP_STEP_64;
	hgo->x_skip = old_hgo->x_skip;
	hgo->y_skip = old_hgo->y_skip;
	hgo->sampling = old_hgo->sampling;

	par->old_addr = old_hgo->addr;
}


static void vspm_if_set_vsp_hgt_param(
	struct vspm_if_vsp_hgt_par_t *par, T_VSP_HGT *old_hgt)
{
	struct vsp_hgt_t *hgt = &par->hgt;
	unsigned long *addr;
	int i;

	addr = malloc(800);
	hgt->hard_addr = 0;
	hgt->virt_addr = (void *)addr;
	hgt->width = old_hgt->width;
	hgt->height = old_hgt->height;
	hgt->x_offset = old_hgt->x_offset;
	hgt->y_offset = old_hgt->y_offset;
	hgt->x_skip = old_hgt->x_skip;
	hgt->y_skip = old_hgt->y_skip;
	for (i = 0; i < 6; i++) {
		hgt->area[i].lower = old_hgt->area[i].lower;
		hgt->area[i].upper = old_hgt->area[i].upper;
	}
	hgt->sampling = old_hgt->sampling;

	par->old_addr = old_hgt->addr;
}


static void vspm_if_set_vsp_ctrl_param(
	struct vspm_if_vsp_ctrl_par_t *par, T_VSP_CTRL *old_ctrl)
{
	struct vsp_ctrl_t *ctrl = &par->ctrl;

	if (old_ctrl->sru != NULL) {
		vspm_if_set_vsp_sru_param(&par->sru, old_ctrl->sru);
		ctrl->sru = &par->sru;
	}

	if (old_ctrl->uds != NULL) {
		vspm_if_set_vsp_uds_param(&par->uds, old_ctrl->uds);
		ctrl->uds = &par->uds;
	}

	if (old_ctrl->lut != NULL) {
		vspm_if_set_vsp_lut_param(&par->lut, old_ctrl->lut);
		ctrl->lut = &par->lut.lut;
	}

	if (old_ctrl->clu != NULL) {
		vspm_if_set_vsp_clu_param(&par->clu, old_ctrl->clu);
		ctrl->clu = &par->clu.clu;
	}

	if (old_ctrl->hst != NULL) {
		vspm_if_set_vsp_hst_param(&par->hst, old_ctrl->hst);
		ctrl->hst = &par->hst;
	}

	if (old_ctrl->hsi != NULL) {
		vspm_if_set_vsp_hsi_param(&par->hsi, old_ctrl->hsi);
		ctrl->hsi = &par->hsi;
	}

	if (old_ctrl->bru != NULL) {
		vspm_if_set_vsp_bru_param(&par->bru, old_ctrl->bru);
		ctrl->bru = &par->bru.bru;
	}

	if (old_ctrl->hgo != NULL) {
		vspm_if_set_vsp_hgo_param(&par->hgo, old_ctrl->hgo);
		ctrl->hgo = &par->hgo.hgo;
	}

	if (old_ctrl->hgt != NULL) {
		vspm_if_set_vsp_hgt_param(&par->hgt, old_ctrl->hgt);
		ctrl->hgt = &par->hgt.hgt;
	}
}


static void vspm_if_set_vsp_param(
	struct vspm_if_vsp_par_t *vsp_par, T_VSP_START *src)
{
	struct vsp_start_t *st_par = &vsp_par->start_par;

	st_par->rpf_num = src->rpf_num;
	st_par->rpf_order = 0;
	st_par->use_module = src->use_module;

	if (src->src1_par != NULL) {
		vspm_if_set_vsp_src_param(&vsp_par->src[0], src->src1_par);
		st_par->src_par[0] = &vsp_par->src[0].src_par;
	}

	if (src->src2_par != NULL) {
		vspm_if_set_vsp_src_param(&vsp_par->src[1], src->src2_par);
		st_par->src_par[1] = &vsp_par->src[1].src_par;
	}

	if (src->src3_par != NULL) {
		vspm_if_set_vsp_src_param(&vsp_par->src[2], src->src3_par);
		st_par->src_par[2] = &vsp_par->src[2].src_par;
	}

	if (src->src4_par != NULL) {
		vspm_if_set_vsp_src_param(&vsp_par->src[3], src->src4_par);
		st_par->src_par[3] = &vsp_par->src[3].src_par;
	}

	if (src->dst_par != NULL) {
		vspm_if_set_vsp_dst_param(&vsp_par->dst_par, src->dst_par);
		st_par->dst_par = &vsp_par->dst_par;
	}

	if (src->ctrl_par != NULL) {
		vspm_if_set_vsp_ctrl_param(&vsp_par->ctrl, src->ctrl_par);
		st_par->ctrl_par = &vsp_par->ctrl.ctrl;
	}

	st_par->dl_par.hard_addr = 0;
	st_par->dl_par.virt_addr = NULL;
	if (vsp_par->dst_par.width == 0) {
		st_par->dl_par.tbl_num = 192;
	} else {
		st_par->dl_par.tbl_num =
			192 + 64 * (((vsp_par->dst_par.width + 255) / 256) - 1);
	}

	vsp_par->job_par.type = VSPM_TYPE_VSP_AUTO;
	vsp_par->job_par.par.vsp = st_par;
}


static void vspm_if_copy_histogram(struct vspm_if_vsp_cb_t *cb_info)
{
	struct vspm_if_vsp_hgo_par_t *hgo_par;
	struct vspm_if_vsp_hgt_par_t *hgt_par;

	hgo_par = &cb_info->vsp_par.ctrl.hgo;
	if ((hgo_par->hgo.virt_addr != NULL) &&
		(hgo_par->old_addr != NULL)) {
		memcpy(hgo_par->old_addr, hgo_par->hgo.virt_addr, 768);
	}

	hgt_par = &cb_info->vsp_par.ctrl.hgt;
	if ((hgt_par->hgt.virt_addr != NULL) &&
		(hgt_par->old_addr != NULL)) {
		memcpy(hgt_par->old_addr, hgt_par->hgt.virt_addr, 768);
	}
}


static void vspm_if_release_memory(struct vspm_if_vsp_cb_t *cb_info)
{
	struct vspm_if_vsp_src_par_t *src_par;
	struct vspm_if_vsp_ctrl_par_t *ctrl_par = &cb_info->vsp_par.ctrl;

	int i;

	/* RPF(CLUT) */
	for (i = 0; i < 4; i++) {
		src_par = &cb_info->vsp_par.src[i];
		if (src_par->clut.virt_addr != NULL) {
			free(src_par->clut.virt_addr);
		}
	}

	/* LUT */
	if (ctrl_par->lut.lut.lut.hard_addr != 0) {
		(void)mmngr_free_in_user_ext(ctrl_par->lut.fd);
	}

	/* CLU */
	if (ctrl_par->clu.clu.clu.hard_addr != 0) {
		(void)mmngr_free_in_user_ext(ctrl_par->clu.fd);
	}

	/* HGO */
	if (ctrl_par->hgo.hgo.virt_addr != NULL) {
		free(ctrl_par->hgo.hgo.virt_addr);
	}

	/* HGT */
	if (ctrl_par->hgt.hgt.virt_addr != NULL) {
		free(ctrl_par->hgt.hgt.virt_addr);
	}

	free(cb_info);
}


long VSPM_lib_DriverInitialize(unsigned long *handle)
{
	struct vspm_if_vsp_handle_t *hdl;

	struct vspm_init_t init_par;
	long ercd;

	/* check parameter */
	if (handle == NULL) {
		return R_VSPM_NG;
	}

	/* allocate memory */
	hdl = malloc(sizeof(struct vspm_if_vsp_handle_t));
	if (hdl == NULL) {
		return R_VSPM_NG;
	}
	memset(hdl, 0, sizeof(struct vspm_if_vsp_handle_t));

	/* initialize mutex */
	(void)pthread_mutex_init(&hdl->mutex, NULL);

	/* set parameter */
	init_par.use_ch = VSPM_EMPTY_CH;
	init_par.mode = VSPM_MODE_MUTUAL;
	init_par.type = VSPM_TYPE_VSP_AUTO;
	init_par.par.vsp = NULL;

	/* initialize VSP manager */
	ercd = vspm_init_driver(&hdl->handle, &init_par);
	if (ercd != R_VSPM_OK) {
		free(hdl);
	}

	*handle = (unsigned long)hdl;

	return ercd;
}


long VSPM_lib_DriverQuit(unsigned long handle)
{
	struct vspm_if_vsp_handle_t *hdl =
		(struct vspm_if_vsp_handle_t *)handle;
	struct vspm_if_vsp_cb_t *cb, *next_cb;

	long ercd;

	/* check parameter */
	if (hdl == NULL) {
		return R_VSPM_PARAERR;
	}

	/* finalize VSP manager */
	ercd = vspm_quit_driver(hdl->handle);
	if (ercd != R_VSPM_NG) {
		return ercd;
	}

	/* release memory of unnotified callback */
	cb = hdl->cb_list;
	while (cb != NULL) {
		next_cb = cb->next;
		ercd = vspm_if_del_vsp_cb_info(cb);
		if (ercd == R_VSPM_OK) {
			vspm_if_release_memory(cb);
		}
		cb = next_cb;
	}

	/* release memory */
	free(hdl);

	return R_VSPM_OK;
}


static void vspm_if_vsp_cb_func(
	unsigned long job_id, long result, void *user_data)
{
	struct vspm_if_vsp_cb_t *cb_info =
		(struct vspm_if_vsp_cb_t *)user_data;
	long ercd;

	/* check parameter */
	if (cb_info == NULL) {
		return;
	}

	/* disconnect callback information from handle */
	ercd = vspm_if_del_vsp_cb_info(cb_info);
	if (ercd != R_VSPM_OK) {
		/* already cancel? */
		return;
	}

	/* copy histogram */
	vspm_if_copy_histogram(cb_info);

	/* callback function */
	if (cb_info->cb_func != NULL) {
		cb_info->cb_func(job_id, result, cb_info->user_data);
	}

	/* release memory */
	vspm_if_release_memory(cb_info);
}


long VSPM_lib_Entry(
	unsigned long handle,
	unsigned long *puwJobId,
	char bJobPriority,
	VSPM_IP_PAR *ptIpParam,
	unsigned long uwUserData,
	void *pfnNotifyComplete)
{
	struct vspm_if_vsp_handle_t *hdl =
		(struct vspm_if_vsp_handle_t *)handle;

	struct vspm_if_vsp_cb_t *cb_info;
	long ercd;

	/* check parameter */
	if (hdl == NULL) {
		return R_VSPM_PARAERR;
	}

	if (ptIpParam->uhType != VSPM_TYPE_VSP_AUTO) {
		return R_VSPM_PARAERR;
	}

	if (ptIpParam->unionIpParam.ptVsp == NULL) {
		return R_VSPM_PARAERR;
	}

	/* allocate memory */
	cb_info = malloc(sizeof(struct vspm_if_vsp_cb_t));
	if (cb_info == NULL) {
		return R_VSPM_NG;
	}
	memset(cb_info, 0, sizeof(struct vspm_if_vsp_cb_t));

	/* set entry parameter */
	vspm_if_set_vsp_param(&cb_info->vsp_par, ptIpParam->unionIpParam.ptVsp);

	/* set callback parameter */
	cb_info->user_data = uwUserData;
	cb_info->cb_func = pfnNotifyComplete;

	/* connect callback information to handle */
	vspm_if_add_vsp_cb_info(hdl, cb_info);

	/* entry of job */
	ercd = vspm_entry_job(
		hdl->handle,
		puwJobId,
		bJobPriority,
		&cb_info->vsp_par.job_par,
		(void *)cb_info,
		vspm_if_vsp_cb_func);
	if (ercd != R_VSPM_OK) {
		(void)vspm_if_del_vsp_cb_info(cb_info);
		vspm_if_release_memory(cb_info);
	}

	return ercd;
}


long VSPM_lib_Cancel(unsigned long handle, unsigned long uwJobId)
{
	struct vspm_if_vsp_handle_t *hdl =
		(struct vspm_if_vsp_handle_t *)handle;

	/* check parameter */
	if (hdl == NULL) {
		return R_VSPM_PARAERR;
	}

	return vspm_cancel_job(hdl->handle, uwJobId);
}


short VSPM_lib_PVioUdsGetRatio(
	unsigned short ushInSize,
	unsigned short ushOutSize,
	unsigned short *ushRatio)
{
	unsigned long ratio;

	/* check parameter */
	if (ushRatio == NULL)
		return R_VSPM_NG;

	if ((ushInSize < 1) | (ushOutSize < 1))
		return R_VSPM_NG;

	/* AMD=0 */
	if (ushInSize < ushOutSize) {
		ushInSize--;
		ushOutSize--;
	}

	/* calculate ratio */
	ratio = ((unsigned long)ushInSize) * 0x1000UL;
	ratio /= (unsigned long)ushOutSize;

	/* check error */
	if ((ratio < 0x100) || (ratio > 0xFFFF))
		return R_VSPM_NG;

	*ushRatio = (unsigned short)ratio;

	return R_VSPM_OK;
}
