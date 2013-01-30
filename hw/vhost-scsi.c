/*
 * vhost_scsi host device
 *
 * Copyright IBM, Corp. 2011
 *
 * Authors:
 *  Stefan Hajnoczi   <stefanha@linux.vnet.ibm.com>
 *
 * Changes for QEMU mainline + tcm_vhost kernel upstream:
 *  Nicholas Bellinger <nab@risingtidesystems.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#include <sys/ioctl.h>
#include "config.h"
#include "qemu/queue.h"
#include "monitor/monitor.h"
#include "migration/migration.h"
#include "vhost-scsi.h"
#include "vhost.h"
#include "virtio-scsi.h"

typedef struct VHostSCSI {
    VirtIOSCSICommon vs;

    Error *migration_blocker;

    struct vhost_dev dev;
} VHostSCSI;

static int vhost_scsi_set_endpoint(VirtIODevice *vdev)
{
    VHostSCSI *vs = (VHostSCSI *)vdev;
    struct vhost_scsi_target backend;
    int ret;

    memset(&backend, 0, sizeof(backend));
    pstrcpy(backend.vhost_wwpn, sizeof(backend.vhost_wwpn), vs->vs.conf->wwpn);
    ret = ioctl(vs->dev.control, VHOST_SCSI_SET_ENDPOINT, &backend);
    if (ret < 0) {
        return -errno;
    }
    return 0;
}

static void vhost_scsi_clear_endpoint(VirtIODevice *vdev)
{
    VHostSCSI *vs = (VHostSCSI *)vdev;
    struct vhost_scsi_target backend;

    memset(&backend, 0, sizeof(backend));
    pstrcpy(backend.vhost_wwpn, sizeof(backend.vhost_wwpn), vs->vs.conf->wwpn);
    ioctl(vs->dev.control, VHOST_SCSI_CLEAR_ENDPOINT, &backend);
}

static int vhost_scsi_start(VHostSCSI *vs, VirtIODevice *vdev)
{
    int ret, abi_version;

    ret = ioctl(vs->dev.control, VHOST_SCSI_GET_ABI_VERSION, &abi_version);
    if (ret < 0) {
        return -errno;
    }
    if (abi_version > VHOST_SCSI_ABI_VERSION) {
        error_report("vhost-scsi: The running tcm_vhost kernel abi_version:"
                     " %d is greater than vhost_scsi userspace supports: %d, please"
                     " upgrade your version of QEMU\n", abi_version,
                     VHOST_SCSI_ABI_VERSION);
        return -ENOSYS;
    }

    ret = vhost_dev_enable_notifiers(&vs->dev, vdev);
    if (ret < 0) {
        return ret;
    }

    ret = vhost_dev_start(&vs->dev, vdev);
    if (ret < 0) {
        vhost_dev_disable_notifiers(&vs->dev, vdev);
    }

    return ret;
}

static void vhost_scsi_stop(VHostSCSI *vs, VirtIODevice *vdev)
{
    vhost_dev_stop(&vs->dev, vdev);
}

static void vhost_scsi_set_config(VirtIODevice *vdev,
                                  const uint8_t *config)
{
    VirtIOSCSIConfig *scsiconf = (VirtIOSCSIConfig *)config;
    VHostSCSI *vs = (VHostSCSI *)vdev;

    if ((uint32_t) ldl_raw(&scsiconf->sense_size) != vs->vs.sense_size ||
        (uint32_t) ldl_raw(&scsiconf->cdb_size) != vs->vs.cdb_size) {
        error_report("vhost-scsi does not support changing the sense data and CDB sizes");
        exit(1);
    }
}

static void vhost_scsi_set_status(VirtIODevice *vdev, uint8_t val)
{
    VHostSCSI *vs = (VHostSCSI *)vdev;
    bool start = (val & (VIRTIO_CONFIG_S_DRIVER|VIRTIO_CONFIG_S_DRIVER_OK));

    if (vs->dev.started == start) {
        return;
    }

    if (start) {
        int ret;

        ret = vhost_scsi_start(vs, vdev);
        if (ret < 0) {
            error_report("virtio-scsi: unable to start vhost: %s\n",
                         strerror(-ret));

            /* There is no userspace virtio-scsi fallback so exit */
            exit(1);
        }
    } else {
        vhost_scsi_stop(vs, vdev);
    }
}

VirtIODevice *vhost_scsi_init(DeviceState *dev, VirtIOSCSIConf *proxyconf)
{
    VHostSCSI *vs;
    int vhostfd = -1;
    int ret;

    if (!proxyconf->wwpn) {
        error_report("vhost-scsi: missing wwpn\n");
        return NULL;
    }

    if (proxyconf->vhostfd) {
        vhostfd = monitor_handle_fd_param(cur_mon, proxyconf->vhostfd);
        if (vhostfd == -1) {
            error_report("vhost-scsi: unable to parse vhostfd\n");
            return NULL;
        }
    }

    vs = (VHostSCSI *)virtio_scsi_init_common(dev, proxyconf,
                                              sizeof(VHostSCSI));

    vs->vs.vdev.set_config = vhost_scsi_set_config;
    vs->vs.vdev.set_status = vhost_scsi_set_status;
    vs->vs.vdev.set_vhost_endpoint = vhost_scsi_set_endpoint;
    vs->vs.vdev.clear_vhost_endpoint = vhost_scsi_clear_endpoint;

    vs->dev.nvqs = VHOST_SCSI_VQ_NUM_FIXED + vs->vs.conf->num_queues;
    vs->dev.vqs = g_new(struct vhost_virtqueue, vs->dev.nvqs);

    ret = vhost_dev_init(&vs->dev, vhostfd, "/dev/vhost-scsi", true);
    if (ret < 0) {
        error_report("vhost-scsi: vhost initialization failed: %s\n",
                strerror(-ret));
        return NULL;
    }
    vs->dev.backend_features = 0;
    vs->dev.acked_features = 0;

    error_setg(&vs->migration_blocker,
            "vhost-scsi does not support migration");
    migrate_add_blocker(vs->migration_blocker);

    return &vs->vs.vdev;
}

void vhost_scsi_exit(VirtIODevice *vdev)
{
    VHostSCSI *vs = (VHostSCSI *)vdev;
    migrate_del_blocker(vs->migration_blocker);
    error_free(vs->migration_blocker);

    /* This will stop vhost backend. */
    vhost_scsi_set_status(vdev, 0);
    g_free(vs->dev.vqs);
    virtio_cleanup(vdev);
}

