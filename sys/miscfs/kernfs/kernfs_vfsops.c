/*
 * Copyright (c) 1992 The Regents of the University of California
 * Copyright (c) 1990, 1992 Jan-Simon Pendry
 * All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * %sccs.redist.c%
 *
 *	%W% (Berkeley) %G%
 *
 * $Id: kernfs_vfsops.c,v 1.1 1993/03/23 23:56:54 cgd Exp $
 */

/*
 * Kernel params Filesystem
 */

#include "param.h"
#include "systm.h"
#include "time.h"
#include "types.h"
#include "proc.h"
#include "vnode.h"
#include "mount.h"
#include "namei.h"
#include "malloc.h"
#include "miscfs/kernfs/kernfs.h"

kernfs_init()
{
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_init\n");		/* printed during system boot */
#endif
}

/*
 * Mount the kernel parameter filesystem
 */
kernfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	int error = 0;
	u_int size;
	struct kernfs_mount *fmp;
	struct vnode *rvp;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_mount(mp = %x)\n", mp);
#endif

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	error = getnewvnode(VT_UFS, mp, &kernfs_vnodeops, &rvp);	/* XXX */
	if (error)
		return (error);

	fmp = (struct kernfs_mount *) malloc(sizeof(struct kernfs_mount),
				 M_UFSMNT, M_WAITOK);	/* XXX */
	rvp->v_type = VDIR;
	rvp->v_flag |= VROOT;
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_mount: root vp = %x\n", rvp);
#endif
	fmp->kf_root = rvp;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = (qaddr_t) fmp;
	getnewfsid(mp, MOUNT_KERNFS);

	(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	bzero(mp->mnt_stat.f_mntfromname, MNAMELEN);
	bcopy("kernfs", mp->mnt_stat.f_mntfromname, sizeof("kernfs"));
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_mount: at %s\n", mp->mnt_stat.f_mntonname);
#endif
	return (0);
}

kernfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	return (0);
}

kernfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	int error;
	int flags = 0;
	extern int doforce;
	struct vnode *rootvp = VFSTOKERNFS(mp)->kf_root;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_unmount(mp = %x)\n", mp);
#endif

	if (mntflags & MNT_FORCE) {
		/* kernfs can never be rootfs so don't check for it */
		if (!doforce)
			return (EINVAL);
		flags |= FORCECLOSE;
	}

	/*
	 * Clear out buffer cache.  I don't think we
	 * ever get anything cached at this level at the
	 * moment, but who knows...
	 */
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_unmount: calling mntflushbuf\n");
#endif
	mntflushbuf(mp, 0); 
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_unmount: calling mntinvalbuf\n");
#endif
	if (mntinvalbuf(mp, 1))
		return (EBUSY);
	if (rootvp->v_usecount > 1)
		return (EBUSY);
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_unmount: calling vflush\n");
#endif
	if (error = vflush(mp, rootvp, flags))
		return (error);

#ifdef KERNFS_DIAGNOSTIC
	vprint("kernfs root", rootvp);
#endif	 
	/*
	 * Release reference on underlying root vnode
	 */
	vrele(rootvp);
	/*
	 * And blow it away for future re-use
	 */
	vgone(rootvp);
	/*
	 * Finally, throw away the kernfs_mount structure
	 */
	free(mp->mnt_data, M_UFSMNT);	/* XXX */
	mp->mnt_data = 0;
	return 0;
}

kernfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct vnode *vp;
	int error;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_root(mp = %x)\n", mp);
#endif

	/*
	 * Return locked reference to root.
	 */
	vp = VFSTOKERNFS(mp)->kf_root;
	VREF(vp);
	VOP_LOCK(vp);
	*vpp = vp;
	return (0);
}

kernfs_quotactl(mp, cmd, uid, arg, p)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{
	return (EOPNOTSUPP);
}

kernfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	struct filedesc *fdp;
	int lim;
	int i;
	int last;
	int freefd;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_statfs(mp = %x)\n", mp);
#endif

	sbp->f_type = MOUNT_KERNFS;
	sbp->f_flags = 0;
	sbp->f_fsize = DEV_BSIZE;
	sbp->f_bsize = DEV_BSIZE;
	sbp->f_blocks = 2;		/* 1K to keep df happy */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 0;		/* Allow for "." */
	sbp->f_ffree = 0;		/* See comments above */
	if (sbp != &mp->mnt_stat) {
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	return (0);
}

kernfs_sync(mp, waitfor)
	struct mount *mp;
	int waitfor;
{
	return (0);
}

kernfs_fhtovp(mp, fhp, vpp)
	struct mount *mp;
	struct fid *fhp;
	struct vnode **vpp;
{
	return (EOPNOTSUPP);
}

kernfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	return (EOPNOTSUPP);
}

struct vfsops kernfs_vfsops = {
	kernfs_mount,
	kernfs_start,
	kernfs_unmount,
	kernfs_root,
	kernfs_quotactl,
	kernfs_statfs,
	kernfs_sync,
	kernfs_fhtovp,
	kernfs_vptofh,
	kernfs_init,
};
