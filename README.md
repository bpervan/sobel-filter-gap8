# Sobel Filter on GAP8

## Introduction
This is an implementation of the Sobel Filter on the GAP8 platform. Implementation executes in parallel on 8 cluster cores and is both manually and OpenMP parallelized. The repository also contains testing images.

## What's in this folder:

| Name              |         Description                       |
|-------------------|-------------------------------------------|
|SobelFilter.c      |  Manually parallelized version            |
|SobelFilter.omp.c  |  OpenMP parallelized version              |
|Makefile           |  make script                              |
|README.md          |  Readme file                              |

## How to build and execute the application
Setup the GAP SDK by following instructions provided by the GAP team, available on [GitHub](https://github.com/GreenWaves-Technologies/gap_sdk) or [GAP8 Manual](https://greenwaves-technologies.com/manuals/BUILD/HOME/html/index.html)

After setting up the SDK and running
~~~~~shell
source sourceme.sh
~~~~~
clone this repository to ``$(GAP_SDK_HOME)/applications``.

You should now be fine with running the application. There are a few parameters to play around with, namely:
- ``PMSIS_OS`` - specifies the underlying OS (``freertos`` or ``pulpos``)
- ``platform`` - specifies whether the application will be run in simulator(``gvsoc``) or on hardware(``board``)
- ``io`` - specifies the default output for ``printf`` (``host`` or ``uart``)
- ``CONFIG_OPENMP`` - If set to ``1`` selects OpenMP version. Otherwise, the manually parallelized version is selected (this flag is also used by the ``GAP_SDK``)

Long story short and for example, to compile and run the OpenMP parallelized version, using the simulator, type:
~~~~~sh
make clean && make PMSIS_OS=freertos platform=gvsoc io=host CONFIG_OPENMP=1 all -j4 && make platform=gvsoc io=host run
~~~~~

Input image can be switched by changing the filename under the appropriate variable in source files:
~~~~~sh
char *in_image_file_name = "valve.pgm";
~~~~~
Currently, image dimensions are hardcoded and all of the example images are of the appropriate size.

## Result
Running this app in the simulator should output something like this in Shell:

~~~~~sh
	 *** Sobel Filter (OMP) ***

Main FC entry point
Image ../../../valve.pgm:  [W: 320, H: 240] Bytes per pixel 1, HeaderSize: 15
P5
320 240
255

Image ../../../valve.pgm, [W: 320, H: 240], Bytes per pixel 1, Size: 76800 bytes, Loaded successfully
Main cluster entry point
Total cycles: 7997969
Path to output image: ../../../img_out.ppm
Writing image  [#                             ]
Writing image  [####                          ]
Writing image  [#######                       ]
Writing image  [###########                   ]
Writing image  [##############                ]
Writing image  [#################             ]
Writing image  [#####################         ]
Writing image  [########################      ]
Writing image  [###########################   ]
Cluster closed, over and out
~~~~~

Output (processed) image ``img_out.ppm`` will be in the app folder.
