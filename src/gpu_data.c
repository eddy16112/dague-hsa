#include "include/dague_config.h"
#include "include/gpu_data.h"
#include "include/profiling.h"

#include "data_dist/data_distribution.h"

#if defined(DAGUE_CUDA_SUPPORT)

#if defined(DAGUE_PROFILING)
static int movein_key_start;
static int movein_key_end;
static int compute_key_start;
static int compute_key_end;
static int moveout_key_start;
static int moveout_key_end;
#endif  /* defined(PROFILING) */

/* We don't use gpu_devices, instead we use a subset of gpu-array
 * gpu_array - list of GPU by order of their performance
 */
gpu_device_t** gpu_devices = NULL;

int dague_gpu_init(int* puse_gpu, int dague_show_detailed_capabilities)
{
    int ndevices;
    int i;
    
    if( (*puse_gpu) == -1 ) {
        return -1;  /* Nothing to do around here */
    }
    cuInit(0);
    
    cuDeviceGetCount( &ndevices );
    
    if( ndevices > (*puse_gpu) )
        ndevices = (*puse_gpu);
    /* Update the number of GPU for the upper layer */
    *puse_gpu = ndevices;
    if( 0 == ndevices ) {
        return -1;
    }
    
    dague_data_enable_gpu( ndevices );

    gpu_devices = (gpu_device_t**)calloc(ndevices, sizeof(gpu_device_t));

#if defined(DAGUE_PROFILING)
    dague_profiling_add_dictionary_keyword( "movein", "fill:#33FF33",
                                              &movein_key_start, &movein_key_end);
    dague_profiling_add_dictionary_keyword( "compute", "fill:#ff33cc",
                                              &compute_key_start, &compute_key_end);
    dague_profiling_add_dictionary_keyword( "moveout", "fill:#ffff66",
                                              &moveout_key_start, &moveout_key_end);
#endif  /* defined(PROFILING) */

	for( i = 0; i < ndevices; i++ ) {
        unsigned int total_mem;
        gpu_device_t* gpu_device;
        CUdevprop devProps;
        char szName[256];
        CUresult status;
        int major, minor;
        CUdevice hcuDevice;
        cudaError_t cuda_status;

	    status = cuDeviceGet( &hcuDevice, i );
	    DAGUE_CUDA_CHECK_ERROR( "cuDeviceGet ", status, {ndevices = 0; return -1;} );
        status = cuDeviceGetName( szName, 256, hcuDevice );
        DAGUE_CUDA_CHECK_ERROR( "cuDeviceGetName ", status, {ndevices = 0; return -1;} );

        status = cuDeviceComputeCapability( &major, &minor, hcuDevice);
        DAGUE_CUDA_CHECK_ERROR( "cuDeviceComputeCapability ", status, {ndevices = 0; return -1;} );

        status = cuDeviceGetProperties( &devProps, hcuDevice );
        DAGUE_CUDA_CHECK_ERROR( "cuDeviceGetProperties ", status, {ndevices = 0; return -1;} );

        printf("Device %d (capability %d.%d): %s\n", i, major, minor, szName );
        if( dague_show_detailed_capabilities ) {
            printf("\tmaxThreadsPerBlock : %d\n", devProps.maxThreadsPerBlock );
            printf("\tmaxThreadsDim      : [%d %d %d]\n", devProps.maxThreadsDim[0],
                   devProps.maxThreadsDim[1], devProps.maxThreadsDim[2] );
            printf("\tmaxGridSize        : [%d %d %d]\n", devProps.maxGridSize[0],
                   devProps.maxGridSize[1], devProps.maxGridSize[2] );
            printf("\tsharedMemPerBlock  : %d\n", devProps.sharedMemPerBlock );
            printf("\tconstantMemory     : %d\n", devProps.totalConstantMemory );
            printf("\tSIMDWidth          : %d\n", devProps.SIMDWidth );
            printf("\tmemPitch           : %d\n", devProps.memPitch );
            printf("\tregsPerBlock       : %d\n", devProps.regsPerBlock );
            printf("\tclockRate          : %d\n", devProps.clockRate );
        }
        status = cuDeviceTotalMem( &total_mem, hcuDevice );
        DAGUE_CUDA_CHECK_ERROR( "cuDeviceTotalMem ", status, {ndevices = 0; return -1;} );

        gpu_device = (gpu_device_t*)calloc(1, sizeof(gpu_device_t));
        gpu_devices[i] = gpu_device;
        dague_dequeue_construct(&gpu_device->pending);
        gpu_device->major = major;
        gpu_device->minor = minor;
        
        /* cuCtxCreate: Function works on floating contexts and current context */
        status = cuCtxCreate( &(gpu_device->ctx), 0 /*CU_CTX_BLOCKING_SYNC*/, hcuDevice );
        DAGUE_CUDA_CHECK_ERROR( "(INIT) cuCtxCreate ", status,
                                {free(gpu_device); gpu_devices[i] = NULL; continue; } );

        /**
         * Allocate the streams
         */
        {
            int stream_id;
            gpu_device->max_streams = DAGUE_MAX_STREAMS;
            for( stream_id = 0; stream_id < DAGUE_MAX_STREAMS; stream_id++ ) {
                cuda_status = (cudaError_t)cuStreamCreate( &(gpu_device->streams[stream_id]), 0 );
                DAGUE_CUDA_CHECK_ERROR( "cuStreamCreate ", cuda_status,
                                        ({
                                            gpu_device->max_streams = stream_id - 1;
                                            break;
                                        }) );
            }
        }
        gpu_device->id  = i;
        gpu_device->executed_tasks = 0;
        gpu_device->transferred_data_in = 0;
        gpu_device->transferred_data_out = 0;

        status = cuCtxPopCurrent(NULL);
        DAGUE_CUDA_CHECK_ERROR( "(INIT) cuCtxPopCurrent ", status,
                                {free(gpu_device); return -1;} );

#if defined(DAGUE_PROFILING)
        gpu_device->profiling = dague_profiling_thread_init( 6*4096, "GPU %d.0", i );
#endif  /* defined(PROFILING) */
    }

    return 0;
}

#endif
