/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2002 Cluster File Systems, Inc.
 *   Author: Peter J. Braam <braam@clusterfs.com>
 *   Author: Phil Schwan <phil@clusterfs.com>
 *   Author: Brian Behlendorf <behlendorf1@llnl.gov> 
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
 *
 */

#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#define printk printf


#include <linux/lustre_lib.h>
#include <linux/lustre_idl.h>
#include <linux/lustre_dlm.h>

#include <unistd.h>
#include <sys/un.h>
#include <time.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <asm/page.h>   /* needed for PAGE_SIZE - rread*/ 

#include "parser.h"
#include "lctl.h"

#define __KERNEL__
#include <linux/list.h>
#undef __KERNEL__

static int fd = -1;
static uint64_t conn_addr = -1;
static uint64_t conn_cookie;
static char rawbuf[8192];
static char *buf = rawbuf;
static int max = 8192;

#if 0
static int thread;
#endif

int device_setup(int argc, char **argv) {
        return 0;
}

/* Misc support functions */
static int do_name2dev(char *func, char *name) {
        struct obd_ioctl_data data;
        int rc;

        LUSTRE_CONNECT(func);
        IOCINIT(data);

        data.ioc_inllen1 = strlen(name) + 1;
        data.ioc_inlbuf1 = name;

        if (obd_ioctl_pack(&data, &buf, max)) {
                fprintf(stderr, "error: %s: invalid ioctl\n", cmdname(func));
                return -2;
        }
        rc = ioctl(fd, OBD_IOC_NAME2DEV , buf);
        if (rc < 0) {
                fprintf(stderr, "error: %s: %s - %s\n", cmdname(func),
                        name, strerror(rc = errno));
                return rc;
        }

        memcpy((char *)(&data), buf, sizeof(data));

        return data.ioc_dev + N2D_OFF;
}

/* 
 * resolve a device name to a device number.
 * supports a number or name.  
 * FIXME: support UUID 
 */
static int parse_devname(char * func, char *name) 
{
        int rc;
        int ret = -1;

        if (!name) 
                return ret;
        if (name[0] == '$') {
                rc = do_name2dev(func, name + 1);
                if (rc >= N2D_OFF) {
                        ret = rc - N2D_OFF;
                        printf("%s is device %d\n", name,
                               ret);
                } else {
                        fprintf(stderr, "error: %s: %s: %s\n", cmdname(func),
                                name, "device not found");
                }
                        
        } else
                ret = strtoul(name, NULL, 0);
        return ret;
}


#if 0
/* pack "LL LL LL LL LL LL LL L L L L L L L L L a60 a60 L L L" */
static char * obdo_print(struct obdo *obd)
{
        char buf[1024];

        sprintf(buf, "id: %Ld\ngrp: %Ld\natime: %Ld\nmtime: %Ld\nctime: %Ld\n"
                "size: %Ld\nblocks: %Ld\nblksize: %d\nmode: %o\nuid: %d\n"
                "gid: %d\nflags: %x\nobdflags: %x\nnlink: %d,\nvalid %x\n",
                obd->o_id,
                obd->o_gr,
                obd->o_atime,
                obd->o_mtime,
                obd->o_ctime,
                obd->o_size,
                obd->o_blocks,
                obd->o_blksize,
                obd->o_mode,
                obd->o_uid,
                obd->o_gid,
                obd->o_flags,
                obd->o_obdflags,
                obd->o_nlink,
                obd->o_valid);
        return strdup(buf);
}
#endif

/* Device selection commands */
int jt_dev_newdev(int argc, char **argv) 
{
        int rc;
        struct obd_ioctl_data data;

        LUSTRE_CONNECT(argv[0]);
        IOCINIT(data);

        if (argc != 1)
                return CMD_HELP;

        rc = ioctl(fd, OBD_IOC_NEWDEV , &data);
        if (rc < 0)
                fprintf(stderr, "error: %s: %s\n", cmdname(argv[0]),
                        strerror(rc=errno));
        else {
                printf("Current device set to %d\n", data.ioc_dev);
        }

        return rc;
}

static int do_device(char *func, int dev) {
        struct obd_ioctl_data data;

        memset(&data, 0, sizeof(data));
        data.ioc_dev = dev;
        LUSTRE_CONNECT(func);

        if (obd_ioctl_pack(&data, &buf, max)) {
                CERROR("error: %s: invalid ioctl\n", cmdname(func));
                return -2;
        }

        return ioctl(fd, OBD_IOC_DEVICE , buf);
}

int jt_dev_device(int argc, char **argv) 
{
        int rc, dev;
        do_disconnect(argv[0], 1);

        if (argc != 2)
                return CMD_HELP;
        dev = parse_devname(argv[0], argv[1]);
        if (dev < 0) {
                return -1; 
        }
        rc = do_device(argv[0], dev);
        if (rc < 0)
                fprintf(stderr, "error: %s: %s\n", cmdname(argv[0]),
                        strerror(rc = errno));

        return rc;
}

static int do_uuid2dev(char *func, char *name) {
        return 0;
}

int jt_dev_uuid2dev(int argc, char **argv) 
{
        do_uuid2dev(NULL, NULL);
        return 0;
}

int jt_dev_name2dev(int argc, char **argv) 
{
        int rc;
        if (argc != 2)
                return CMD_HELP;

        rc = do_name2dev(argv[0], argv[1]);
        if (rc >= N2D_OFF) {
                int dev = rc - N2D_OFF;
                rc = do_device(argv[0], dev);
                if (rc == 0)
                        printf("%d\n", dev);
        }
        return rc;
}

int jt_dev_list(int argc, char **argv) 
{
        int rc;
        char buf[1024];
        struct obd_ioctl_data *data = (struct obd_ioctl_data *)buf;

        LUSTRE_CONNECT(argv[0]);
        memset(buf, 0, sizeof(buf));
        data->ioc_version = OBD_IOCTL_VERSION;
        data->ioc_addr = conn_addr;
        data->ioc_cookie = conn_addr;
        data->ioc_len = sizeof(buf);
        data->ioc_inllen1 = sizeof(buf) - size_round(sizeof(*data));

        if (argc != 1)
                return CMD_HELP;
                
        rc = ioctl(fd, OBD_IOC_LIST , data);
        if (rc < 0)
                fprintf(stderr, "error: %s: %s\n", cmdname(argv[0]),
                        strerror(rc=errno));
        else {
                printf("%s", data->ioc_bulk);
        }

        return rc;
}

/* Device configuration commands */
int do_disconnect(char *func, int verbose) 
{
        int rc;
        struct obd_ioctl_data data;

        if (conn_addr == -1) 
                return 0; 

        IOCINIT(data);

        rc = ioctl(fd, OBD_IOC_DISCONNECT , &data);
        if (rc < 0) {
                fprintf(stderr, "error: %s: %x %s\n", cmdname(func),
                        OBD_IOC_DISCONNECT, strerror(errno));
        } else {
                if (verbose)
                        printf("%s: disconnected conn %Lx\n", cmdname(func),
                               conn_addr);
                conn_addr = -1;
        }

        return rc;
}

#if 0
static int jt_dev_newconn(int argc, char **argv)
{
        int rc;a
        struct obd_ioctl_data data;

        IOCINIT(data);
        if (argc != 1) {
                fprintf(stderr, "usage: %s\n", cmdname(argv[0]));
                return -1;
        }

        rc = ioctl(fd, OBD_IOC_RECOVD_NEWCONN , &data);
        if (rc < 0)
                fprintf(stderr, "error: %s: %s\n", cmdname(argv[0]),
                        strerror(rc = errno));

        return rc;
}
#endif

int jt_dev_probe(int argc, char **argv) 
{
        int rc;
        struct obd_ioctl_data data;

        IOCINIT(data);
        do_disconnect(argv[0], 1);

        if (argc != 1)
                return CMD_HELP;

        rc = ioctl(fd, OBD_IOC_CONNECT , &data);
        if (rc < 0)
                fprintf(stderr, "error: %s: %x %s\n", cmdname(argv[0]),
                        OBD_IOC_CONNECT, strerror(rc = errno));
        else
                conn_addr = data.ioc_addr;
                conn_cookie = data.ioc_cookie;
        return rc;
}

int jt_dev_close(int argc, char **argv) 
{
        if (argc != 1)
                return CMD_HELP;

        if (conn_addr == -1)
                return 0;

        return do_disconnect(argv[0], 0);
}

int jt_opt_device(int argc, char **argv)
{
        char *arg2[3];
        int ret;
        int rc;

        if (argc < 3) {
                fprintf(stderr, "usage: %s devno <command [args ...]>\n",
                        cmdname(argv[0]));
                return -1;
        }

        rc = do_device("device", parse_devname(argv[0], argv[1]));

        if (!rc) {
                arg2[0] = "connect";
                arg2[1] = NULL;
                rc = jt_dev_probe(1, arg2);
        }

        if (!rc)
                rc = Parser_execarg(argc - 2, argv + 2, cmdlist);
        ret = do_disconnect(argv[0], 0);
        if (!rc)
                rc = ret;

        return rc;
}


int jt_dev_attach(int argc, char **argv) 
{
        int rc;
        struct obd_ioctl_data data;

        IOCINIT(data);
        if (argc != 2 && argc != 3 && argc != 4)
                return CMD_HELP;

        data.ioc_inllen1 =  strlen(argv[1]) + 1;
        data.ioc_inlbuf1 = argv[1];
        if (argc >= 3) {
                data.ioc_inllen2 = strlen(argv[2]) + 1;
                data.ioc_inlbuf2 = argv[2];
        }

        if (argc == 4) {
                data.ioc_inllen3 = strlen(argv[3]) + 1;
                data.ioc_inlbuf3 = argv[3];
        }

        if (obd_ioctl_pack(&data, &buf, max)) {
                fprintf(stderr, "error: %s: invalid ioctl\n",cmdname(argv[0]));
                return -2;
        }

        rc = ioctl(fd, OBD_IOC_ATTACH , buf);
        if (rc < 0)
                fprintf(stderr, "error: %s: %x %s\n", cmdname(argv[0]),
                        OBD_IOC_ATTACH, strerror(rc = errno));
        else if (argc == 3) {
                char name[1024];
                if (strlen(argv[2]) > 128) {
                        printf("Name too long to set environment\n");
                        return -EINVAL;
                }
                snprintf(name, 512, "LUSTRE_DEV_%s", argv[2]);
                rc = setenv(name, argv[1], 1);
                if (rc) {
                        printf("error setting env variable %s\n", name);
                }
        }

        return rc;
}

int jt_dev_setup(int argc, char **argv) 
{
        int rc;
        struct obd_ioctl_data data;

        IOCINIT(data);
        if (argc > 3)
                return CMD_HELP;
                
        data.ioc_dev = -1;
        if (argc > 1) {
                data.ioc_dev = parse_devname(argv[0], argv[1]);
                if (data.ioc_dev < 0) 
                        return rc = -1;

                data.ioc_inllen1 = strlen(argv[1]) + 1;
                data.ioc_inlbuf1 = argv[1];
        }
        if ( argc == 3 ) {
                data.ioc_inllen2 = strlen(argv[2]) + 1;
                data.ioc_inlbuf2 = argv[2];
        }

        if (obd_ioctl_pack(&data, &buf, max)) {
                fprintf(stderr, "error: %s: invalid ioctl\n", cmdname(argv[0]));
                return -2;
        }
        rc = ioctl(fd, OBD_IOC_SETUP , buf);
        if (rc < 0)
                fprintf(stderr, "error: %s: %s\n", cmdname(argv[0]),
                        strerror(rc = errno));

        return rc;
}

int jt_dev_detach(int argc, char **argv) 
{
        int rc;
        struct obd_ioctl_data data;

        IOCINIT(data);
        if (argc != 1)
                return CMD_HELP;

        if (obd_ioctl_pack(&data, &buf, max)) {
                fprintf(stderr, "error: %s: invalid ioctl\n", cmdname(argv[0]));
                return -2;
        }

        rc = ioctl(fd, OBD_IOC_DETACH , buf);
        if (rc < 0)
                fprintf(stderr, "error: %s: %s\n", cmdname(argv[0]),
                        strerror(rc=errno));

        return rc;
}

int jt_dev_cleanup(int argc, char **argv) 
{
        int rc;
        struct obd_ioctl_data data;

        IOCINIT(data);
        if (argc != 1)
                return CMD_HELP;

        rc = ioctl(fd, OBD_IOC_CLEANUP , &data);
        if (rc < 0)
                CERROR("error: %s: %s\n", cmdname(argv[0]),
                        strerror(rc=errno));
        return rc;
}

int jt_dev_lov_config(int argc, char **argv) 
{
        struct obd_ioctl_data data;
        struct lov_desc desc; 
        uuid_t *uuidarray;
        int rc, size, i;

        IOCINIT(data);
        if (argc <= 5)
                return CMD_HELP;

        if (strlen(argv[1]) > sizeof(uuid_t) - 1) { 
                fprintf(stderr, "lov_config: no %dB memory for uuid's\n", 
                        size);
                return -ENOMEM;
        }
            
        memset(&desc, 0, sizeof(desc)); 
        strcpy(desc.ld_uuid, argv[1]); 
        desc.ld_default_stripecount = strtoul(argv[2], NULL, 0); 
        desc.ld_default_stripesize = strtoul(argv[3], NULL, 0); 
        desc.ld_pattern = strtoul(argv[4], NULL, 0); 
        desc.ld_tgt_count = argc - 5;


        size = sizeof(uuid_t) * desc.ld_tgt_count;
        uuidarray = malloc(size);
        if (!uuidarray) { 
                fprintf(stderr, "lov_config: no %dB memory for uuid's\n", 
                        size);
                return -ENOMEM;
        }
        memset(uuidarray, 0, size); 
        for (i=5 ; i < argc ; i++) { 
                char *buf = (char *) (uuidarray + i -5 );
                if (strlen(argv[i]) >= sizeof(uuid_t)) { 
                        fprintf(stderr, "lov_config: arg %d (%s) too long\n",  
                                i, argv[i]);
                        free(uuidarray);
                        return -EINVAL;
                }
                strcpy(buf, argv[i]); 
        }

        data.ioc_inllen1 = sizeof(desc); 
        data.ioc_inlbuf1 = (char *)&desc;
        data.ioc_inllen2 = size;
        data.ioc_inlbuf2 = (char *)uuidarray;

        if (obd_ioctl_pack(&data, &buf, max)) {
                fprintf(stderr, "error: %s: invalid ioctl\n",cmdname(argv[0]));
                return -EINVAL;
        }

        rc = ioctl(fd, OBD_IOC_LOV_CONFIG , buf);
        if (rc < 0)
                fprintf(stderr, "lov_config: error: %s: %s\n", 
                        cmdname(argv[0]),strerror(rc = errno));
        free(uuidarray);
        return rc;
}

#if 0
int jt_dev_create(int argc, char **argv) {
        struct obd_ioctl_data data;
        struct timeval next_time;
        int count = 1, next_count;
        int verbose;
        int i;

        IOCINIT(data);
        if (argc < 2 || argc > 4)
                return CMD_HELP;

        count = strtoul(argv[1], NULL, 0);

        if (argc > 2)
                data.ioc_obdo1.o_mode = strtoul(argv[2], NULL, 0);
        else
                data.ioc_obdo1.o_mode = 0100644;
        data.ioc_obdo1.o_valid = OBD_MD_FLMODE;

        verbose = get_verbose(argv[3]);

        printf("%s: %d obdos\n", cmdname(argv[0]), count);
        gettimeofday(&next_time, NULL);
        next_time.tv_sec -= verbose;

        for (i = 1, next_count = verbose; i <= count ; i++) {
                rc = ioctl(fd, OBD_IOC_CREATE , &data);
                if (rc < 0) {
                        fprintf(stderr, "error: %s: #%d - %s\n",
                                cmdname(argv[0]), i, strerror(rc = errno));
                        break;
                }
                if (be_verbose(verbose, &next_time, i, &next_count, count))
                        printf("%s: #%d is object id %Ld\n", cmdname(argv[0]),
                               i, data.ioc_obdo1.o_id);
        }
        return rc;
}

int jt_dev_destroy(int argc, char **argv) {
        struct obd_ioctl_data data;

        IOCINIT(data);
        if (argc != 2) {
                fprintf(stderr, "usage: %s id\n", cmdname(argv[0]));
                return -1;
        }

        data.ioc_obdo1.o_id = strtoul(argv[1], NULL, 0);
        data.ioc_obdo1.o_mode = S_IFREG|0644;

        rc = ioctl(fd, OBD_IOC_DESTROY , &data);
        if (rc < 0)
                fprintf(stderr, "error: %s: %s\n", cmdname(argv[0]),
                        strerror(rc = errno));

        return rc;
}
#endif

/* Device configuration commands */
int jt_dev_setattr(int argc, char **argv) 
{
        int rc;
        struct obd_ioctl_data data;

        IOCINIT(data);
        if (argc != 2)
                return CMD_HELP;
                
        data.ioc_obdo1.o_id = strtoul(argv[1], NULL, 0);
        data.ioc_obdo1.o_mode = S_IFREG | strtoul(argv[2], NULL, 0);
        data.ioc_obdo1.o_valid = OBD_MD_FLMODE;

        rc = ioctl(fd, OBD_IOC_SETATTR , &data);
        if (rc < 0)
                fprintf(stderr, "error: %s: %s\n", cmdname(argv[0]),
                        strerror(rc = errno));

        return rc;
}

int jt_dev_getattr(int argc, char **argv) 
{
        int rc;
        struct obd_ioctl_data data;

        if (argc != 2)
                return CMD_HELP;

        IOCINIT(data);
        data.ioc_obdo1.o_id = strtoul(argv[1], NULL, 0);
        /* to help obd filter */
        data.ioc_obdo1.o_mode = 0100644;
        data.ioc_obdo1.o_valid = 0xffffffff;
        printf("%s: object id %Ld\n", cmdname(argv[0]), data.ioc_obdo1.o_id);

        rc = ioctl(fd, OBD_IOC_GETATTR , &data);
        if (rc) {
                fprintf(stderr, "error: %s: %s\n", cmdname(argv[0]),
                        strerror(rc=errno));
        } else {
                printf("%s: object id %Ld, mode %o\n", cmdname(argv[0]),
                       data.ioc_obdo1.o_id, data.ioc_obdo1.o_mode);
        }
        return rc;
}

int jt_dev_test_getattr(int argc, char **argv) 
{
        int rc;
        struct obd_ioctl_data data;
        struct timeval start, next_time;
        int i, count, next_count;
        int verbose;

        if (argc != 2 && argc != 3)
                return CMD_HELP;

        IOCINIT(data);
        count = strtoul(argv[1], NULL, 0);

        if (argc == 3)
                verbose = get_verbose(argv[2]);
        else
                verbose = 1;

        data.ioc_obdo1.o_valid = 0xffffffff;
        data.ioc_obdo1.o_id = 2;
        gettimeofday(&start, NULL);
        next_time.tv_sec = start.tv_sec - verbose;
        next_time.tv_usec = start.tv_usec;
        printf("%s: getting %d attrs (testing only): %s", cmdname(argv[0]),
               count, ctime(&start.tv_sec));

        for (i = 1, next_count = verbose; i <= count; i++) {
                rc = ioctl(fd, OBD_IOC_GETATTR , &data);
                if (rc < 0) {
                        fprintf(stderr, "error: %s: #%d - %s\n",
                                cmdname(argv[0]), i, strerror(rc = errno));
                        break;
                } else {
                        if (be_verbose(verbose, &next_time, i,&next_count,count))
                        printf("%s: got attr #%d\n", cmdname(argv[0]), i);
        	}
	}

        if (!rc) {
                struct timeval end;
                double diff;

                gettimeofday(&end, NULL);

                diff = difftime(&end, &start);

                --i;
                printf("%s: %d attrs in %.4gs (%.4g attr/s): %s",
                       cmdname(argv[0]), i, diff, (double)i / diff,
                       ctime(&end.tv_sec));
        }
        return rc;
}

int jt_dev_test_brw(int argc, char **argv) 
{
        int rc;
        struct obd_ioctl_data data;
        struct timeval start, next_time;
        char *bulk, *b;
        int pages = 1, obdos = 1, count, next_count;
        int verbose = 1, write = 0, rw;
        int i, o, p;
        int len;

        if (argc < 2 || argc > 6)
                return CMD_HELP;

        count = strtoul(argv[1], NULL, 0);

        if (argc >= 3) {
                if (argv[2][0] == 'w' || argv[2][0] == '1')
                        write = 1;
                else if (argv[2][0] == 'r' || argv[2][0] == '0')
                        write = 0;

                verbose = get_verbose(argv[3]);
        }

        if (argc >= 5)
                pages = strtoul(argv[4], NULL, 0);
        if (argc >= 6)
                obdos = strtoul(argv[5], NULL, 0);

        if (obdos != 1 && obdos != 2) {
                fprintf(stderr, "error: %s: only 1 or 2 obdos supported\n",
                        cmdname(argv[0]));
                return -2;
        }

        len = pages * PAGE_SIZE;

        bulk = calloc(obdos, len);
        if (!bulk) {
                fprintf(stderr,"error: %s: no memory allocating %dx%d pages\n",
                        cmdname(argv[0]), obdos, pages);
                return -2;
        }
        IOCINIT(data);
        data.ioc_obdo1.o_id = 2;
        data.ioc_count = len;
        data.ioc_offset = 0;
        data.ioc_plen1 = len;
        data.ioc_pbuf1 = bulk;
        if (obdos > 1) {
                data.ioc_obdo2.o_id = 3;
                data.ioc_plen2 = len;
                data.ioc_pbuf2 = bulk + len;
        }

        gettimeofday(&start, NULL);
        next_time.tv_sec = start.tv_sec - verbose;
        next_time.tv_usec = start.tv_usec;

        printf("%s: %s %d (%dx%d pages) (testing only): %s",
               cmdname(argv[0]), write ? "writing" : "reading",
               count, obdos, pages, ctime(&start.tv_sec));

        /*
         * We will put in the start time (and loop count inside the loop)
         * at the beginning of each page so that we will be able to validate
         * (at some later time) whether the data actually made it or not.
         *
         * XXX we do not currently use any of this memory in OBD_IOC_BRW_*
         *     just to avoid the overhead of the copy_{to,from}_user.  It
         *     can be fixed if we ever need to send real data around.
         */
        for (o = 0, b = bulk; o < obdos; o++)
                for (p = 0; p < pages; p++, b += PAGE_SIZE)
                        memcpy(b, &start, sizeof(start));

        rw = write ? OBD_IOC_BRW_WRITE : OBD_IOC_BRW_READ;
        for (i = 1, next_count = verbose; i <= count; i++) {
                if (write) {
                        b = bulk + sizeof(struct timeval);
                        for (o = 0; o < obdos; o++)
                                for (p = 0; p < pages; p++, b += PAGE_SIZE)
                                        memcpy(b, &count, sizeof(count));
                }

                rc = ioctl(fd, rw, &data);
                if (rc) {
                        fprintf(stderr, "error: %s: #%d - %s on %s\n",
                                cmdname(argv[0]), i, strerror(rc = errno),
                                write ? "write" : "read");
                        break;
                } else if (be_verbose(verbose,&next_time,i,&next_count,count))
                        printf("%s: %s number %d\n", cmdname(argv[0]),
                               write ? "write" : "read", i);
        }

        free(bulk);

        if (!rc) {
                struct timeval end;
                double diff;

                gettimeofday(&end, NULL);

                diff = difftime(&end, &start);

                --i;
                printf("%s: %s %dx%dx%d pages in %.4gs (%.4g pg/s): %s",
                       cmdname(argv[0]), write ? "wrote" : "read", obdos,
                       pages, i, diff, (double)obdos * i * pages / diff,
                       ctime(&end.tv_sec));
        }
        return rc;
}

int jt_dev_test_ldlm(int argc, char **argv) 
{
        int rc;
        struct obd_ioctl_data data;

        IOCINIT(data);
        if (argc != 1)
                return CMD_HELP;

        rc = ioctl(fd, IOC_LDLM_TEST, &data);
        if (rc)
                fprintf(stderr, "error: %s: test failed: %s\n",
                        cmdname(argv[0]), strerror(rc = errno));
        return rc;
}

