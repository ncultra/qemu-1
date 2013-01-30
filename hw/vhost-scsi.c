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
#include "vhost-scsi.h"
#include "vhost.h"

struct VHostSCSI {
    const char *id;
    const char *wwpn;
    uint16_t tpgt;
    int vhostfd;
    struct vhost_dev dev;
    struct vhost_virtqueue vqs[VHOST_SCSI_VQ_NUM];
    QLIST_ENTRY(VHostSCSI) list;
};

static QLIST_HEAD(, VHostSCSI) vhost_scsi_list =
    QLIST_HEAD_INITIALIZER(vhost_scsi_list);

VHostSCSI *find_vhost_scsi(const char *id)
{
    VHostSCSI *vs;

    QLIST_FOREACH(vs, &vhost_scsi_list, list) {
        if (!strcmp(id, vs->id)) {
            return vs;
        }
    }
    return NULL;
}

const char *vhost_scsi_get_id(VHostSCSI *vs)
{
    return vs->id;
}

int vhost_scsi_start(VHostSCSI *vs, VirtIODevice *vdev)
{
    int ret, abi_version;
    struct vhost_scsi_target backend;

    if (!vhost_dev_query(&vs->dev, vdev)) {
        return -ENOTSUP;
    }

    vs->dev.nvqs = VHOST_SCSI_VQ_NUM;
    vs->dev.vqs = vs->vqs;

    ret = vhost_dev_enable_notifiers(&vs->dev, vdev);
    if (ret < 0) {
        return ret;
    }

    ret = vhost_dev_start(&vs->dev, vdev);
    if (ret < 0) {
        return ret;
    }

    ret = ioctl(vs->dev.control, VHOST_SCSI_GET_ABI_VERSION, &abi_version);
    if (ret < 0) {
        ret = -errno;
        vhost_dev_stop(&vs->dev, vdev);
        return ret;
    }
    if (abi_version > VHOST_SCSI_ABI_VERSION) {
        error_report("vhost-scsi: The running tcm_vhost kernel abi_version:"
		" %d is greater than vhost_scsi userspace supports: %d, please"
		" upgrade your version of QEMU\n", abi_version,
		VHOST_SCSI_ABI_VERSION);
        ret = -ENOSYS;
        vhost_dev_stop(&vs->dev, vdev);
        return ret;
    }
    fprintf(stdout, "TCM_vHost ABI version: %d\n", abi_version);

    memset(&backend, 0, sizeof(backend));
    pstrcpy(backend.vhost_wwpn, sizeof(backend.vhost_wwpn), vs->wwpn);
    backend.vhost_tpgt = vs->tpgt;
    ret = ioctl(vs->dev.control, VHOST_SCSI_SET_ENDPOINT, &backend);
    if (ret < 0) {
        ret = -errno;
        vhost_dev_stop(&vs->dev, vdev);
        return ret;
    }

    return 0;
}

void vhost_scsi_stop(VHostSCSI *vs, VirtIODevice *vdev)
{
    int ret;
    struct vhost_scsi_target backend;

    memset(&backend, 0, sizeof(backend));
    pstrcpy(backend.vhost_wwpn, sizeof(backend.vhost_wwpn), vs->wwpn);
    backend.vhost_tpgt = vs->tpgt;
    ret = ioctl(vs->dev.control, VHOST_SCSI_CLEAR_ENDPOINT, &backend);
    if (ret < 0) {
        error_report("vhost-scsi: Failed to clear endpoint\n");
    }

    vhost_dev_stop(&vs->dev, vdev);
}

static VHostSCSI *vhost_scsi_add(const char *id, const char *wwpn,
                                 uint16_t tpgt, const char *vhostfd_str)
{
    VHostSCSI *vs;
    int ret;

    vs = g_malloc0(sizeof(*vs));
    if (!vs) {
        error_report("vhost-scsi: unable to allocate *vs\n");
        return NULL;
    }
    vs->vhostfd = -1;

    if (vhostfd_str) {
        vs->vhostfd = monitor_handle_fd_param(cur_mon, vhostfd_str);
        if (vs->vhostfd == -1) {
            error_report("vhost-scsi: unable to parse vs->vhostfd\n");
            return NULL;
        }
    }
    /* TODO set up vhost-scsi device and bind to tcm_vhost/$wwpm/tpgt_$tpgt */
    ret = vhost_dev_init(&vs->dev, vs->vhostfd, "/dev/vhost-scsi", false);
    if (ret < 0) {
        error_report("vhost-scsi: vhost initialization failed: %s\n",
                strerror(-ret));
        return NULL;
    }
    vs->dev.backend_features = 0;
    vs->dev.acked_features = 0;

    vs->id = g_strdup(id);
    vs->wwpn = g_strdup(wwpn);
    vs->tpgt = tpgt;
    QLIST_INSERT_HEAD(&vhost_scsi_list, vs, list);

    return vs;
}

VHostSCSI *vhost_scsi_add_opts(QemuOpts *opts)
{
    const char *id;
    const char *wwpn, *vhostfd;
    uint64_t tpgt;

    id = qemu_opts_id(opts);
    if (!id) {
        error_report("vhost-scsi: no id specified\n");
        return NULL;
    }
    if (find_vhost_scsi(id)) {
        error_report("duplicate vhost-scsi: \"%s\"\n", id);
        return NULL;
    }

    wwpn = qemu_opt_get(opts, "wwpn");
    if (!wwpn) {
        error_report("vhost-scsi: \"%s\" missing wwpn\n", id);
        return NULL;
    }

    tpgt = qemu_opt_get_number(opts, "tpgt", UINT64_MAX);
    if (tpgt > UINT16_MAX) {
        error_report("vhost-scsi: \"%s\" needs a 16-bit tpgt\n", id);
        return NULL;
    }
    vhostfd = qemu_opt_get(opts, "vhostfd");

    return vhost_scsi_add(id, wwpn, tpgt, vhostfd);
}
