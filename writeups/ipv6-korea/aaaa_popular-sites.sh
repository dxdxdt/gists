#!/bin/bash
# From https://ko.semrush.com/trending-websites/kr/all
declare -ra SITES=(
	"youtube.com"
	"google.com"
	"naver.com"
	"dcinside.com"
	"namu.wiki"
	"coupang.com"
	"fmkorea.com"
	"daum.net"
	"tistory.com"
	"kakao.com"
	"kr-weathernews.com"
	"arca.live"
	"ilbe.com"
	"inven.co.kr"
	"fabulouslink.xyz"
	"ruliweb.com"
	"aliexpress.com"
	"twitter.com"
	"facebook.com"
	"instagram.com"
	"tvwiki.top"
	"nate.com"
	"dogdrip.net"
	"donga.com"
	"draplay.info"
	"gmarket.co.kr"
	"google.co.kr"
	"sauceflex.com"
	"twitch.tv"
	"ppomppu.co.kr"
	"nexon.com"
	"humoruniv.com"
	"ssg.com"
	"theqoo.net"
	"av19.org"
	"sflex.us"
	"yadongtube.net"
	"linkkf.app"
	"buzzvil.com"
	"newtoki315.com"
	"wikipedia.org"
	"manatoki315.net"
	"chosun.com"
	"newtoki314.com"
	"manatoki314.net"
	"avsee.in"
	"newtoki313.com"
	"appier.net"
	"manatoki313.net"
	"pornhub.com"
	"etoland.co.kr"
	"samsung.com"
	"yatv.net"
	"ravielink.xyz"
	"afreecatv.com"
	"newtoki316.com"
	"lotteon.com"
	"auction.co.kr"
	"clien.net"
	"danawa.com"
	"openai.com"
	"postype.com"
	"manatoki316.net"
	"netflix.com"
	"linkmine.co.kr"
	"musinsa.com"
	"instiz.net"
	"oliveyoung.co.kr"
	"tvmon.help"
	"shinhancard.com"
	"twidouga.net"
	"inlcorp.com"
	"bobaedream.co.kr"
	"asianhd1.com"
	"cjlogistics.com"
	"tdgall.com"
	"yadongkorea.red"
	"mediacategory.com"
	"kbcard.com"
	"dhlottery.co.kr"
	"op.gg"
	"adbrix.io"
	"xvideos.com"
	"jmana.one"
	"gezip.net"
	"tossbank.com"
	"booktoki315.com"
	"brunch.co.kr"
	"newspic.kr"
	"anilife.live"
	"ygosu.com"
	"watchfreejavonline.co"
	"fmkorea.org"
	"booktoki314.com"
	"mk.co.kr"
	"hasha.in"
	"avdbs.com"
	"ssfshop.com"
	"tgd.kr"
)

w=0
for s in ${SITES[@]}
do
	if [ ${#s} -gt $w ]; then
		w=${#s}
	fi
done

cnt=0
for s in ${SITES[@]}
do
	aaaa=( $(dig +short AAAA "$s") )
	tpl=${aaaa[@]}
	tpl=${tpl// /,}

	[ ${#aaaa[@]} -gt 0 ] && let 'cnt+=1'

	printf '%-'$w's\t%s' $s $tpl
	echo # MacOS
done

echo "$cnt/${#SITES[@]}" >&2
