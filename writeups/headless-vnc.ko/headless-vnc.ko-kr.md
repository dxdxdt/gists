# 클라우드 리눅스 VM에 GUI 돌리기

AWS EC2나 GCP CE의 윈도 머신은 대부분 RDP로 접근하는 반면에, 리눅스 머신은 모두 CUI로만
사용하도록 되어있음. 리눅스는 셸로 모든 작업을 할 수 있지만 유니티 에디터같은 소프트웨어는 GUI없이
거의 사용이 불가능하다. 머신에 하드웨어를 추가할 수 없는 환경이라면 이 방법을 추천한다.

## 개요
**Xorg X11 dummy video**를 사용해서 가상 fb를 만들고, **TigerVNC X 윈도 서버 모듈**을
사용해 원격에서 접속할 수 있도록 설정한다. 이 문서는 VM에 GDE를 설치하는 방법을 다룬다. 이 문서는
VNC 포트를 외부에 공개하지 않고 SSH 터널링을 사용해 서버 VNC에 연결하는 방법을 설명한다.

## 방법
### SSH 접속
원격 VNC 포트를 로컬로 터널링한다. 포트 2개를 사용하는 이유는 추후에 설명:
```
ssh -L15900:127.0.0.1:5900 -L15901:127.0.0.1:5901 <주소>
```

### 패키지 설치
RPM:

```xorg-x11-drv-dummy tigervnc-server-module gnome-shell gnome-terminal```

DEB: (TODO)

### 설정

**/etc/X11/xorg.conf.d/00-dummy-vnc-video.conf** (새로 만들기):
```
Section "Module"
	Load "vnc"
EndSection

Section "Device"
    Identifier  "Configured Video Device"
    Driver      "dummy"
EndSection

Section "Monitor"
    Identifier  "Configured Monitor"
    HorizSync 31.5-48.5
    VertRefresh 50-70
EndSection

Section "Screen"
    Identifier  "Default Screen"
    Monitor     "Configured Monitor"
    Device      "Configured Video Device"
    DefaultDepth 24
    SubSection "Display"
        Depth 24
        Modes "1280x720"
    EndSubSection

    Option "SecurityTypes" "None"
    Option "AlwaysShared" "true"
EndSection
```

해상도나 색상 깊이를 알맞게 변경한다.

`Option "SecurityTypes" "None"` 행은 VNC 접속 시
암호를 묻지 않도록 설정한다는 의미이다. 암호를 설정하고 싶다면 [이 문서](https://wiki.archlinux.org/index.php/TigerVNC#Expose_the_local_display_directly)
를 참조.

`Option "AlwaysShared" "true"` 행은 동시 접속을 허용한다는 의미. 원치 않으면 주석처리.

VNC 포트를 외부에 노출하여 사용하는 것은 안전하지 않다. TigerVNC가 TLS를 지원하나, X11 모듈로
사용되어 X11 설정으로 옵션을 넘겨주어야 해서 복잡한 설정이 어려울 것이다. 따라서 저자는 VNC 포트를
개방하지 않고 SSH 터널링으로 VNC 접속하는 방법을 택하였다.

### X 서버 실행
```
systemctl set-default graphical.target
systemctl start graphical.target
```

Systemd를 사용하지 않는 배포판에서는 gdm을 enable, start하거나 runlevel을 5로 설정하는 등의
작업을 하여 gdm을 실행한다.

### 접속, 로그인
**127.0.0.1:15900**에 접속하여 원하는 계정으로 로그인한다. 로그인에 성공하면 VNC로 보이는
화면이 빈 화면으로 유지되면 정상. **127.0.0.1:15901**로 접속하면 로그인된 GUI 세션을 이용할 수
있다.

## 외부 링크
* https://lxtreme.nl/blog/headless-x11/
* https://wiki.archlinux.org/index.php/TigerVNC#Expose_the_local_display_directly
