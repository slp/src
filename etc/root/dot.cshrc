alias mail Mail
set history=1000
set path=(/sbin /usr/sbin /bin /usr/bin .)

# directory stuff: cdpath/cd/back
set cdpath=(/sys /usr/src/{bin,sbin,usr.{bin,sbin},pgrm,lib,libexec,share,contrib,local,devel,games,old,})
alias	cd	'set old=$cwd; chdir \!*'
alias	h	history
alias	j	jobs -l
alias	ll	ls -lg
alias	ls	ls -g -k
alias	back	'set back=$old; set old=$cwd; cd $back; unset back; dirs'

alias	z	suspend
alias	x	exit
alias	pd	pushd
alias	pd2	pushd +2
alias	pd3	pushd +3
alias	pd4	pushd +4
alias	df	df -k
alias	du 	du -k
alias	tset	'set noglob histchars=""; eval `\tset -s \!*`; unset noglob histchars'

if ($?prompt) then
	set prompt="`hostname -s`# "
endif
