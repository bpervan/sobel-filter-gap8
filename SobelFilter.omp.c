#include <stdio.h>

/* PMSIS includes. */
#include "pmsis.h"

/* OMP */
#include "omp.h"

/* Gap_lib includes. */
#include "gaplib/ImgIO.h"

/* Image dimensions */
#define IMG_LINES 240
#define IMG_COLS 320

#define STACK_SIZE 2048

/* For convenience */
typedef unsigned char byte;

/* Pointers in memory for input and output images */
byte* ImageIn_L2;
byte* ImageOut_L2;

int G_X[] = {
    -1, 0, 1,
    -2, 0, 2,
    -1, 0, 1
};

int G_Y[] = {
    -1, -2, -1,
    0, 0, 0,
    1, 2, 1
};

/**
 * GAP8 does not implement math library (or it certainly doesn't do so for at least part of it)
 * This (reused) implementation is appropriate for embedded devices
 * See: https://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
 */
uint32_t SquareRoot(uint32_t a_nInput)
{
    uint32_t op  = a_nInput;
    uint32_t res = 0;
    //1u << 14 for uint16_t type;
    uint32_t one = 1uL << 30;

    while (one > op)
    {
        one >>= 2;
    }

    while (one != 0)
    {
        if (op >= res + one)
        {
            op = op - (res + one);
            res = res +  2 * one;
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

void subpicture(byte* in_picture, byte* out_subpicture, int picture_size, int width, int curr_element)
{
    byte is_top_row_pixel = curr_element - width < 0;
    byte is_left_column_pixel = curr_element % width == 0;
    byte is_bottom_row_pixel = curr_element + width >= picture_size;
    byte is_right_column_pixel = (curr_element + 1) % width == 0;

    out_subpicture[0] = !is_top_row_pixel && !is_left_column_pixel      ? in_picture[curr_element - width - 1]  : 0;
    out_subpicture[1] = !is_top_row_pixel                               ? in_picture[curr_element - width]      : 0;
    out_subpicture[2] = !is_top_row_pixel && !is_right_column_pixel     ? in_picture[curr_element - width + 1]  : 0;
    
    out_subpicture[3] = !is_left_column_pixel                           ? in_picture[curr_element - 1]          : 0;
    out_subpicture[4] =                                                   in_picture[curr_element];
    out_subpicture[5] = !is_right_column_pixel                          ? in_picture[curr_element + 1]          : 0;
    
    out_subpicture[6] = !is_bottom_row_pixel && !is_left_column_pixel   ? in_picture[curr_element + width - 1]  : 0;
    out_subpicture[7] = !is_bottom_row_pixel                            ? in_picture[curr_element + width]      : 0;
    out_subpicture[8] = !is_bottom_row_pixel && !is_right_column_pixel  ? in_picture[curr_element + width + 1]  : 0;
    
}

/* Main cluster entry point, executed on core 0 */
void cluster_entry_point(void* args)
{
    printf("Main cluster entry point\n");

    /* Benchmarking. Count active cycles */
    pi_perf_conf(1 << PI_PERF_ACTIVE_CYCLES);
    pi_perf_reset();
    pi_perf_start();
    int time1 = pi_perf_read(PI_PERF_ACTIVE_CYCLES);
    
    int i,j;

    int accu_x;
    int accu_y;

    byte tmp_subpicture[9];
    memset(tmp_subpicture, 0, 9);

    /* This directive will parallelize on all of the cluster cores */
    #pragma omp parallel for firstprivate(tmp_subpicture)
    for(i = 0; i < IMG_LINES * IMG_COLS; ++i){
        subpicture(ImageIn_L2, tmp_subpicture, IMG_LINES * IMG_COLS, IMG_COLS, i);

        accu_x = 0;
        accu_y = 0;

        for(j = 0; j < 9; ++j){
            accu_x = accu_x + (tmp_subpicture[j] * G_X[9 - j - 1]);
            accu_y = accu_y + (tmp_subpicture[j] * G_Y[9 - j - 1]);
        }

        ImageOut_L2[i] = (byte) SquareRoot(accu_x * accu_x + accu_y * accu_y);
    }
    

    /* Stop the counter and print # active cycles */
    pi_perf_stop();
    int time2 = pi_perf_read(PI_PERF_ACTIVE_CYCLES);
    printf("Total cycles: %d\n", time2 - time1);
}



/* Entry point - Executes on FC */
void sobel_filter_main()
{
    printf("Main FC entry point\n");

    char *in_image_file_name = "valve.pgm";
    char path_to_in_image[64];
    sprintf(path_to_in_image, "../../../%s", in_image_file_name);

    int image_size_bytes = IMG_COLS * IMG_LINES * sizeof(byte);

    /* Allocate memory for both input and output images in L2 memory */
    ImageIn_L2 = (byte *) pi_l2_malloc(image_size_bytes);
    ImageOut_L2 = (byte *) pi_l2_malloc(image_size_bytes);

    if(ReadImageFromFile(path_to_in_image, IMG_COLS, IMG_LINES, 1, ImageIn_L2, image_size_bytes, IMGIO_OUTPUT_CHAR, 0))
    {
        printf("Failed to load image %s\n", path_to_in_image);
        pmsis_exit(-1);
    }

    /* Prepare cluster description structure and open cluster */
    struct pi_device cl_device;
    struct pi_cluster_conf cl_configuration;
    pi_cluster_conf_init(&cl_configuration);
    cl_configuration.id = 0;
    pi_open_from_conf(&cl_device, &cl_configuration);
    if(pi_cluster_open(&cl_device))
    {
        printf("Cluster open failed\n");
        pmsis_exit(-1);
    }

    /* Prepare task description structure */
    struct pi_cluster_task * cl_task = pmsis_l2_malloc(sizeof(struct pi_cluster_task));
    memset(cl_task, 0, sizeof(struct pi_cluster_task));
    cl_task->entry = cluster_entry_point;
    cl_task->arg = NULL;
    cl_task->stack_size = (uint32_t) STACK_SIZE;

    /* Send task to cluster, block until completion */
    pi_cluster_send_task_to_cl(&cl_device, cl_task);

    /* Write image to file */
    char *out_image_file_name = "img_out.ppm";
    char path_to_out_image[50];
    sprintf(path_to_out_image, "../../../%s", out_image_file_name);
    printf("Path to output image: %s\n", path_to_out_image);
    WriteImageToFile(path_to_out_image, IMG_COLS, IMG_LINES, 1, ImageOut_L2, GRAY_SCALE_IO);

    pi_cluster_close(&cl_device);

    printf("Cluster closed, over and out\n");

    pmsis_exit(0);
}

/* PMSIS main function */
int main(int argc, char *argv[])
{
    printf("\n\n\t *** Sobel Filter (OMP) ***\n\n");
    return pmsis_kickoff((void *) sobel_filter_main);
}
