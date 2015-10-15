#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "vspm_public.h"
#include "mmngr_user_public.h"

#define FB_ENABLE	1

long fbfd;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;

unsigned int output_hard;
void* output_virt;
MMNGR_ID out_fd;

unsigned int dl_hard;
void* dl_virt;
MMNGR_ID dl_fd;

static pthread_mutex_t g_mut = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
volatile unsigned char end_flag = 0;

static int open_fb(void);
static int close_fb(void);
static void output_fb(void);
static int allocate_memory(void);
static int release_memory(void);

/* callback function */
static void cb_func(
	unsigned long job_id, long result, unsigned long user_data)
{
	pthread_mutex_lock(&g_mut);
	end_flag = 1;
	pthread_cond_signal(&g_cond);
	pthread_mutex_unlock(&g_mut);
}

/* main function */
int main(int argc, char *argv[])
{
	struct vspm_init_t init_par;

	struct vspm_job_t vspm_ip;
	struct vsp_start_t vsp_par;

	struct vsp_src_t src_par[5];
	struct vsp_alpha_unit_t alpha_par;
	struct vsp_dst_t dst_par;
	struct vsp_ctrl_t ctrl_par;

	struct vsp_bru_t bru_par;
	struct vsp_bld_ctrl_t bru_ctrl_par;
	struct vsp_bld_vir_t bru_vir_par;

	unsigned long handle;
	unsigned long jobid;

	long ercd;

#if (FB_ENABLE==1)
	if (open_fb()) {
		printf("open fb failed!!\n");
		return -1;
	}
#else
	vinfo.xres = 1920;
	vinfo.yres = 1080;
	vinfo.bits_per_pixel = 32;
#endif

	if (allocate_memory()) {
		printf("allocate memory failed!!\n");
		(void)close_fb();
		return -1;
	}

	memset(&init_par, 0, sizeof(struct vspm_init_t));
	init_par.use_ch = VSPM_EMPTY_CH;
	init_par.mode = VSPM_MODE_MUTUAL;
	init_par.type = VSPM_TYPE_VSP_AUTO;

	/* Initilise VSP manager */
	ercd = vspm_init_driver(&handle, &init_par);
	if (ercd) {
		printf("vspm_init_driver() Failed!! ercd=%d\n", ercd);
		(void)release_memory();
		(void)close_fb();
		return -1;
	}

	{
		alpha_par.addr_a	= NULL;
		alpha_par.stride_a	= 0;
		alpha_par.swap		= VSP_SWAP_NO;
		alpha_par.asel		= VSP_ALPHA_NUM5;
		alpha_par.aext		= VSP_AEXT_EXPAN;
		alpha_par.anum0		= 0;
		alpha_par.anum1		= 0;
		alpha_par.afix		= 0xff;
		alpha_par.irop		= NULL;
		alpha_par.ckey		= NULL;
		alpha_par.mult		= NULL;

		src_par[0].addr			= NULL;
		src_par[0].addr_c0		= NULL;
		src_par[0].addr_c1		= NULL;
		src_par[0].stride 		= (vinfo.xres/3) * 4;
		src_par[0].stride_c		= 0;
		src_par[0].width		= vinfo.xres/3;
		src_par[0].height		= vinfo.yres/3;
		src_par[0].width_ex		= 0;
		src_par[0].height_ex	= 0;
		src_par[0].x_offset		= 0;
		src_par[0].y_offset		= 0;
		src_par[0].format		= VSP_IN_ARGB8888;
		src_par[0].swap			= VSP_SWAP_NO;
		src_par[0].x_position	= (vinfo.xres/6);
		src_par[0].y_position	= (vinfo.yres/6);
		src_par[0].pwd			= VSP_LAYER_CHILD;
		src_par[0].cipm			= VSP_CIPM_0_HOLD;
		src_par[0].cext			= VSP_CEXT_EXPAN;
		src_par[0].csc			= VSP_CSC_OFF;
		src_par[0].iturbt		= VSP_ITURBT_709;
		src_par[0].clrcng		= VSP_FULL_COLOR;
		src_par[0].vir			= VSP_VIR;
		src_par[0].vircolor		= 0x00FF0000;	/* red */
		src_par[0].clut			= NULL;
		src_par[0].alpha		= &alpha_par;
		src_par[0].connect		= VSP_BRU_USE;

		memcpy(&src_par[1], &src_par[0], sizeof(struct vsp_src_t));
		src_par[1].x_position	= (vinfo.xres/3);
		src_par[1].y_position	= (vinfo.yres/3);
		src_par[1].vircolor		= 0x0000FF00;	/* green */

		memcpy(&src_par[2], &src_par[0], sizeof(struct vsp_src_t));
		src_par[2].x_position	= (vinfo.xres/2);
		src_par[2].y_position	= (vinfo.yres/2);
		src_par[2].vircolor		= 0x000000FF;	/* blue */

		memcpy(&src_par[3], &src_par[0], sizeof(struct vsp_src_t));
		src_par[3].x_position	= (vinfo.xres/2);
		src_par[3].y_position	= (vinfo.yres/6);
		src_par[3].vircolor		= 0x0000000;	/* black */

		memcpy(&src_par[4], &src_par[0], sizeof(struct vsp_src_t));
		src_par[4].x_position	= (vinfo.xres/6);
		src_par[4].y_position	= (vinfo.yres/2);
		src_par[4].vircolor		= 0x00FFFFFF;	/* white */
	}

	{
		dst_par.addr		= (void*)(unsigned long)output_hard;
		dst_par.addr_c0		= NULL;
		dst_par.addr_c1		= NULL;
		dst_par.stride		= vinfo.xres * vinfo.bits_per_pixel / 8;
		dst_par.stride_c	= 0;
		dst_par.width		= vinfo.xres;
		dst_par.height		= vinfo.yres;
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
	}

	{	/* BRU */
		memset(&bru_ctrl_par, 0, sizeof(struct vsp_bld_ctrl_t));
		bru_ctrl_par.rbc			= VSP_RBC_BLEND;
		bru_ctrl_par.blend_coefx	= VSP_COEFFICIENT_BLENDX5;
		bru_ctrl_par.blend_coefy	= VSP_COEFFICIENT_BLENDY5;
		bru_ctrl_par.acoefx 		= VSP_COEFFICIENT_ALPHAX5;
		bru_ctrl_par.acoefy 		= VSP_COEFFICIENT_ALPHAY5;
		bru_ctrl_par.acoefx_fix		= 0x80;
		bru_ctrl_par.acoefy_fix		= 0x80;

		memset(&bru_vir_par, 0, sizeof(struct vsp_bld_vir_t));
		bru_vir_par.width		= vinfo.xres;
		bru_vir_par.height  	= vinfo.yres;
		bru_vir_par.x_position 	= 0;
		bru_vir_par.y_position 	= 0;
		bru_vir_par.pwd			= VSP_LAYER_PARENT;
		bru_vir_par.color		= 0x80808080;

		memset(&bru_par, 0, sizeof(struct vsp_bru_t));
		bru_par.lay_order		= (VSP_LAY_VIRTUAL |
									(VSP_LAY_1 << 4) |
									(VSP_LAY_2 << 8) |
									(VSP_LAY_3 << 12) |
									(VSP_LAY_4 << 16) |
									(VSP_LAY_5 << 20));
		bru_par.blend_virtual 	= &bru_vir_par;
		bru_par.blend_unit_a	= &bru_ctrl_par;
		bru_par.blend_unit_b	= &bru_ctrl_par;
		bru_par.blend_unit_c	= &bru_ctrl_par;
		bru_par.blend_unit_d	= &bru_ctrl_par;
		bru_par.blend_unit_e	= &bru_ctrl_par;
		bru_par.connect			= 0;		/* connect WPF */
	}

	memset(&ctrl_par, 0, sizeof(struct vsp_ctrl_t));
	ctrl_par.bru	= &bru_par;

	vsp_par.rpf_num		= 5;
	vsp_par.use_module	= VSP_BRU_USE;
	vsp_par.src_par[0]	= &src_par[0];
	vsp_par.src_par[1]	= &src_par[1];
	vsp_par.src_par[2]	= &src_par[2];
	vsp_par.src_par[3]	= &src_par[3];
	vsp_par.src_par[4]	= &src_par[4];
	vsp_par.dst_par		= &dst_par;
	vsp_par.ctrl_par	= &ctrl_par;
	vsp_par.dl_par.hard_addr = (void*)(unsigned long)dl_hard;
	vsp_par.dl_par.virt_addr = dl_virt;
	vsp_par.dl_par.tbl_num = 128+64*8;

	memset(&vspm_ip, 0, sizeof(struct vspm_job_t));
	vspm_ip.type = VSPM_TYPE_VSP_AUTO;
	vspm_ip.par.vsp = &vsp_par;

	end_flag = 0;
	ercd = vspm_entry_job(handle, &jobid, 126, &vspm_ip, 0, cb_func); 
	if (ercd) {
		printf("vspm_job_entry() Failed!! ercd=%d\n", ercd);
		(void)vspm_quit_driver(handle);
		(void)release_memory();
		(void)close_fb();
		return -1;
	}

	pthread_mutex_lock(&g_mut);
	if (!end_flag)
		pthread_cond_wait(&g_cond, &g_mut);
	pthread_mutex_unlock(&g_mut);

#if (FB_ENABLE==1)
	/* output frame buffer */
	output_fb();
#endif

	/* Finalise VSP manager */
	ercd = vspm_quit_driver(handle);
	if (ercd) {
		printf("vspm_quit_driver() Failed!! ercd=%d\n", ercd);
	}

	if (release_memory()) {
		printf("release memory failed!!\n");
	}

#if (FB_ENABLE==1)
	if (close_fb()) {
		printf("close fb failed!!\n");
	}
#endif

	return 0;
}

static int open_fb(void)
{
	int ercd;

	fbfd = open("/dev/fb0", O_RDWR);
	if (fbfd < 0) {
		printf("open() failed!! ercd=%d\n", fbfd);
		return -1;
	}

	ercd = ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
	if (ercd) {
		printf("ioctl(FBIOGET_FSCREENINFO) Failed!! ercd=%d\n", ercd);
		close(fbfd);
		return -1;
	}

	ercd = ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);
	if (ercd) {
		printf("ioctl(FBIOGET_VSCREENINFO) Failed!! ercd=%d\n", ercd);
		close(fbfd);
		return -1;
	}

	vinfo.xres = 1920;
	vinfo.yres = 1080;
	vinfo.bits_per_pixel = 32;

	return 0;
}

static int close_fb(void)
{
	int ercd;

	ercd = close(fbfd);
	if (ercd) {
		printf("close() Failed!! ercd=%d\n", ercd);
		return -1;
	}

	return 0;
}

/* output frame buffer */
static void output_fb(void)
{
	char *fbp;

	int ercd;
	unsigned long screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

	fbp = (char*)mmap(0, screensize, PROT_READ|PROT_WRITE, MAP_SHARED, fbfd, 0);
	if (fbp == NULL) {
		printf("mmap() Failed!!");
		close(fbfd);
		return;
	}

	memcpy(fbp, (unsigned char*)output_virt, screensize);

	ercd = ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
	if (ercd) {
		printf("ioctl(FBIOPAN_DISPLAY) Failed!! ercd=%d\n", ercd);
	}
}

static int allocate_memory(void)
{
	int ercd;
	unsigned long screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

	/* output buffer */
	ercd = mmngr_alloc_in_user_ext(
		&out_fd, screensize, &output_hard, &output_virt, MMNGR_VA_SUPPORT, NULL);
	if (ercd) {
		return ercd;
	}
	memset((void*)output_virt, 0, screensize);

	/* display list */
	ercd = mmngr_alloc_in_user_ext(
		&dl_fd, (128+64*8)*8, &dl_hard, &dl_virt, MMNGR_VA_SUPPORT, NULL);
	if (ercd) {
		mmngr_free_in_user_ext(out_fd);
		return ercd;
	}
	memset((void*)dl_virt, 0, (128+64*8)*8);

	return 0;
}

static int release_memory(void)
{
	mmngr_free_in_user_ext(dl_fd);
	mmngr_free_in_user_ext(out_fd);

	return 0;
}
