#include <rpc/rpc.h>
#include <rpcsvc/yppasswd.h>

bool_t
xdr_passwd(xdrs, objp)
XDR *xdrs;
x_passwd *objp;
{
	if (!xdr_string(xdrs, &objp->pw_name, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pw_passwd, ~0)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->pw_uid)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->pw_gid)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pw_gecos, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pw_dir, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pw_shell, ~0)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_yppasswd(xdrs, objp)
XDR *xdrs;
yppasswd *objp;
{
	if (!xdr_string(xdrs, &objp->oldpass, ~0)) {
		return (FALSE);
	}
	if (!xdr_passwd(xdrs, &objp->newpw)) {
		return (FALSE);
	}
	return (TRUE);
}
