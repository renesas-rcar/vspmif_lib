
Sample programs for VSP Manager

## Precondition

# kernel modules

	insmod vspm.ko
	insmod vspm_if.ko
	insmod mmngr.ko
	insmod mmngrbuf.ko

	Please refer to following URL about each modules.
	https://github.com/renesas-rcar

# monitor

	Please connect a HDMI monitor.


## Sample programs

# vspm_tp

 * Use VSP2 IP
 * Renesas near-lossless compression
 * Conversion from YUV to RGB
 * Blending ( BRU )
 * Note:
 *   When use Renesas near-lossless compression, need the space reserved as CMA
 *   area for Lossy compression feature.
 *   It's setting is enabled only YUV planar image format by default setting.

# vspm_tp_colorkey

 * Use VSP2 IP
 * Blending ( BRU )
 * Use color keying mode ( Make green transparent )

# vspm_tp_uds

 * Use VSP2 IP
 * Up scaling ( UDS )
 * Conversion from YUV to RGB

# fdpm_tp

 * Use FDP1 IP
 * Converts from the interlaced video to progressive video
 * ( file out_data_*.yuv is created )
