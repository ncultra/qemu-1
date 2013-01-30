/*
 * vhost_scsi host device
 *
 * Copyright IBM, Corp. 2011
 *
 * Authors:
 *  Stefan Hajnoczi   <stefanha@linux.vnet.ibm.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#ifndef VHOST_SCSI_H
#define VHOST_SCSI_H

#include "qemu-common.h"
#include "qemu/option.h"
#include "qdev.h"

/*
 * Used by QEMU userspace to ensure a consistent vhost-scsi ABI.
 *
 * ABI Rev 0: July 2012 version starting point for v3.6-rc merge candidate +
 *            RFC-v2 vhost-scsi userspace.  Add GET_ABI_VERSION ioctl usage
 */

#define VHOST_SCSI_ABI_VERSION	0

/* TODO #include <linux/vhost.h> properly */
/* For VHOST_SCSI_SET_ENDPOINT/VHOST_SCSI_CLEAR_ENDPOINT ioctl */
struct vhost_scsi_target {
    int abi_version;
    char vhost_wwpn[224];
    unsigned short vhost_tpgt;
    unsigned short reserved;
};

enum vhost_scsi_vq_list {
    VHOST_SCSI_VQ_CTL = 0,
    VHOST_SCSI_VQ_EVT = 1,
    VHOST_SCSI_VQ_IO = 2,
    VHOST_SCSI_VQ_NUM = 3,
};

#define VHOST_VIRTIO 0xAF
#define VHOST_SCSI_SET_ENDPOINT _IOW(VHOST_VIRTIO, 0x40, struct vhost_scsi_target)
#define VHOST_SCSI_CLEAR_ENDPOINT _IOW(VHOST_VIRTIO, 0x41, struct vhost_scsi_target)
#define VHOST_SCSI_GET_ABI_VERSION _IOW(VHOST_VIRTIO, 0x42, int)

extern PropertyInfo qdev_prop_vhost_scsi;

#define DEFINE_PROP_VHOST_SCSI(_n, _s, _f) \
    DEFINE_PROP(_n, _s, _f, qdev_prop_vhost_scsi, VHostSCSI*)

VHostSCSI *find_vhost_scsi(const char *id);
const char *vhost_scsi_get_id(VHostSCSI *vs);
VHostSCSI *vhost_scsi_add_opts(QemuOpts *opts);
int vhost_scsi_start(VHostSCSI *vs, VirtIODevice *vdev);
void vhost_scsi_stop(VHostSCSI *vs, VirtIODevice *vdev);

#endif
