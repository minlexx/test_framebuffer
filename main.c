#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>

#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/fb.h>

#include <FreeImage.h>


static int                        fb =  0;
static struct fb_fix_screeninfo   fb_fix;
static struct fb_var_screeninfo   fb_var;
static unsigned char             *fb_mem;
static int			              fb_mem_offset = 0;


int fb_init()
{
    unsigned long page_mask = 0L;
    
    const char *device = "/dev/fb0";
    
    page_mask = getpagesize() - 1;
    
    fprintf(stderr, "trying fbdev: %s ...\n", device);

    /* get current settings (which we have to restore) */
    if (-1 == (fb = open(device, O_RDWR /* O_WRONLY */))) {
        fprintf(stderr, "open %s: %s\n", device, strerror(errno));
        exit(1);
    }
    
    if (-1 == ioctl(fb, FBIOGET_VSCREENINFO, &fb_var)) {
        perror("ioctl FBIOGET_VSCREENINFO");
        exit(1);
    }
    
    if (-1 == ioctl(fb, FBIOGET_FSCREENINFO, &fb_fix)) {
        perror("ioctl FBIOGET_FSCREENINFO");
        exit(1);
    }
    
    fprintf(stderr, "OK\n");
    
    fprintf(stderr, "Fix: id = %s\n", fb_fix.id);
    fprintf(stderr, "Fix: smem_start = %08X, len = %u\n", (unsigned int)fb_fix.smem_start, fb_fix.smem_len);
    fprintf(stderr, "Fix: type = %d\n", fb_fix.type);
    fprintf(stderr, "Fix: visual = %d\n", fb_fix.visual);
    fprintf(stderr, "Fix: accel = %d\n", fb_fix.accel);
    fprintf(stderr, "Fix: cap = %d\n", fb_fix.capabilities);
    fprintf(stderr, "Fix: pan steps: x, y: %d, %d\n", fb_fix.xpanstep, fb_fix.ypanstep);
    fprintf(stderr, "Fix: ywrap step: %d\n", fb_fix.ywrapstep);
    fprintf(stderr, "Fix: line_length, bytes: %d\n", fb_fix.line_length);
    
    fprintf(stderr, "Var: resolution: %d x %d\n", fb_var.xres, fb_var.yres);
    fprintf(stderr, "Var: virt. resolution: %d x %d\n", fb_var.xres_virtual, fb_var.yres_virtual);
    fprintf(stderr, "Var: offsets x, y: %d, %d\n", fb_var.xoffset, fb_var.yoffset);
    fprintf(stderr, "Var: vmode = %d\n", fb_var.vmode);
    fprintf(stderr, "Var: bpp = %d\n", fb_var.bits_per_pixel);
    fprintf(stderr, "var: RGBA lengths: r, g, b, a: %d, %d, %d, %d\n", fb_var.red.length, fb_var.green.length, fb_var.blue.length, fb_var.transp.length);
    fprintf(stderr, "var: RGBA offsets: r, g, b, a: %d, %d, %d, %d\n", fb_var.red.offset, fb_var.green.offset, fb_var.blue.offset, fb_var.transp.offset);
    
    if (fb_fix.type != FB_TYPE_PACKED_PIXELS) {
        fprintf(stderr,"can handle only packed pixel frame buffers (type != FB_TYPE_PACKED_PIXELS)\n");
        return -1;
    }
    if (fb_fix.visual != FB_VISUAL_TRUECOLOR) {
        fprintf(stderr, "can handle only true color format (visual != FB_VISUAL_TRUECOLOR)\n");
        return -1;
    }
    
    fprintf(stderr, "page_mask = %08X\n", (unsigned int)page_mask);
    
    fb_mem_offset = (unsigned long)(fb_fix.smem_start) & page_mask;
    fprintf(stderr, "fb_mem_offset = %08X\n", (unsigned int)fb_mem_offset);
    
    if ((fb_fix.smem_len < 1) || (fb_fix.smem_start < 1)) {
        fprintf(stderr, "Probably, it does not make sense to map invalid memory!\nThe error will follow.\n");
    }
    
    fb_mem = mmap(NULL,
                  fb_fix.smem_len + fb_mem_offset,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  fb,
                  0);
    if (-1L == (long)fb_mem) {
        perror("mmap");
        return -1;
    }
    
    fprintf(stderr, "Mapped framebuffer mem to %0zX\n", (size_t)fb_mem);
    
    /* move viewport to upper left corner */
    if (fb_var.xoffset != 0 || fb_var.yoffset != 0) {
        fb_var.xoffset = 0;
        fb_var.yoffset = 0;
        if (-1 == ioctl(fb, FBIOPAN_DISPLAY, &fb_var)) {
            perror("ioctl FBIOPAN_DISPLAY");
            return -1;
        }
    }
    
    fb_var.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_ALL | FB_ACTIVATE_FORCE;
    if (ioctl(fb, FBIOPUT_VSCREENINFO, &fb_var) < 0) {
        perror("ioctl FBIOPUT_VSCREENINFO failed");
    } else {
        fprintf(stderr, "activateDisplay OK\n");
    }
    
    return 0;
}

void fb_close()
{
    if (fb_mem != 0) {
        munmap(fb_mem, fb_fix.smem_len + fb_mem_offset);
        fb_mem = NULL;
        fprintf(stderr, "unmapped framebuffer mem\n");
    }
    close(fb);
}


struct mdp_rect {
	unsigned x;
	unsigned y;
	unsigned w;
	unsigned h;
};

struct mdp_display_commit {
	unsigned flags;
	unsigned wait_for_finish;
	struct fb_var_screeninfo var;
	struct mdp_rect roi;
};


void msmfb_display_commit(int fd)
{
#define MSMFB_IOCTL_MAGIC 'm'
#define MSMFB_DISPLAY_COMMIT _IOW(MSMFB_IOCTL_MAGIC, 164, struct mdp_display_commit)
#define MDP_DISPLAY_COMMIT_OVERLAY	1

    struct mdp_display_commit info;
    memset(&info, 0, sizeof(struct mdp_display_commit));
    info.flags = MDP_DISPLAY_COMMIT_OVERLAY;
    
    if (ioctl(fd, MSMFB_DISPLAY_COMMIT, &info) == -1) {
        perror("Failed to call ioctl MSMFB_DISPLAY_COMMIT");
    }
}

void fb_drawsomething()
{
    srand(time(NULL));
    int i = 0;
    for (i = 0; i < 300000; i += 4 ) {
        fb_mem[i + 0] = 0xFF;  // R
        fb_mem[i + 1] = 0x00;  // G
        fb_mem[i + 2] = 0x00;  // B
        fb_mem[i + 3] = 0xFF;  // A (ignored)
    }
    msmfb_display_commit(fb);
}

void displayImage(FREE_IMAGE_FORMAT fmt, char *path)
{
    FIBITMAP *img = FreeImage_Load(fmt, path, 0);
    if (!img) {
        fprintf(stderr, "FreeImage failed to load: %s\n", path);
        return;
    }
    
    unsigned img_bpp = FreeImage_GetBPP(img);
    
    if (img_bpp != fb_var.bits_per_pixel) {
        fprintf(stderr, "Image file BPP (%d) != FB BPP (%d)\n",
            img_bpp, fb_var.bits_per_pixel);
        FreeImage_Unload(img);
        return;
    }
    
    unsigned img_w = FreeImage_GetWidth(img);
    unsigned img_h = FreeImage_GetHeight(img);
    
    if (img_w > fb_var.xres) img_w = fb_var.xres;
    if (img_h > fb_var.yres) img_h = fb_var.yres;
    
    unsigned char *dest_scanline = fb_mem; // initial destination
    for(int y = 0; y < img_h; y++) {
        unsigned char *pixels = FreeImage_GetScanLine(img, y);
        memcpy(dest_scanline, pixels, img_w * img_bpp / 8);
        dest_scanline += fb_fix.line_length; /* goto next line */
    }
    
    FreeImage_Unload(img);
    
    msmfb_display_commit(fb);
}

int main(int argc, char *argv[])
{
    if (fb_init() == -1) {
        fprintf(stderr, "Failed to init framebuffer!\n");
        return 1;
    }
    FreeImage_Initialise(FALSE);
    
    fb_drawsomething();
    
    printf("Sleeping for 10s...\n");
    sleep(10);
    
    displayImage(FIF_PNG, "/data/logo.png");
    sleep(10);
    
    FreeImage_DeInitialise();
    fb_close();
    return 0;
}
