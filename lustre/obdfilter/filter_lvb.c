/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  linux/fs/obdfilter/filter_log.c
 *
 *  Copyright (c) 2001-2003 Cluster File Systems, Inc.
 *   Author: Peter Braam <braam@clusterfs.com>
 *   Author: Andreas Dilger <adilger@clusterfs.com>
 *   Author: Phil Schwan <phil@clusterfs.com>
 *
 *   This file is part of Lustre, http://www.lustre.org.
 *
 *   Lustre is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   Lustre is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Lustre; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define DEBUG_SUBSYSTEM S_FILTER

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>

#include <portals/list.h>
#include <linux/obd_class.h>
#include <linux/lustre_dlm.h>

#include "filter_internal.h"

static int filter_lvbo_init(struct ldlm_resource *res)
{
        int rc;
        struct ost_lvb *lvb = NULL;
        struct obd_device *obd;
        struct obdo *oa = NULL;
        struct dentry *dentry;
        ENTRY;

        LASSERT(res);

        /* we only want lvb's for object resources */
        /* check for internal locks: these have name[1] != 0 */
        if (res->lr_name.name[1])
                RETURN(0);

        down(&res->lr_lvb_sem);
        if (res->lr_lvb_data)
                GOTO(out, rc = 0);

        OBD_ALLOC(lvb, sizeof(*lvb));
        if (!lvb)
                GOTO(out, rc = -ENOMEM);

        res->lr_lvb_data = lvb;
        res->lr_lvb_len = sizeof(*lvb);

        obd = res->lr_namespace->ns_lvbp;
        LASSERT(obd); /* not supposed to fail */

        oa = obdo_alloc();
        if (!oa)
                GOTO(out, rc = -ENOMEM);

        oa->o_id = res->lr_name.name[0];
        oa->o_gr = 0;
        dentry = filter_oa2dentry(obd, oa);
        if (IS_ERR(dentry))
                GOTO(out, PTR_ERR(dentry));

        /* Limit the valid bits in the return data to what we actually use */
        oa->o_valid = OBD_MD_FLID;
        obdo_from_inode(oa, dentry->d_inode, FILTER_VALID_FLAGS);
        f_dput(dentry);

        lvb->lvb_size = dentry->d_inode->i_size;
        lvb->lvb_time = dentry->d_inode->i_mtime;

 out:
        if (oa)
                obdo_free(oa);
        if (rc && lvb) {
                OBD_FREE(lvb, sizeof(*lvb));
                res->lr_lvb_data = NULL;
                res->lr_lvb_len = 0;
        }
        up(&res->lr_lvb_sem);
        return rc;
}



struct ldlm_valblock_ops filter_lvbo = {
        lvbo_init: filter_lvbo_init,
};
