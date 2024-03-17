# Google Calendar Flairs
구글 켈린터 기능 중 하나인 이벤트 이름에 따라 "flair" 이미지를 표시하는 기능이
있다[^1]. 이 이미지를 빼오는 과정을 적어보았음.


이미지가 apk에 다 포함되어 있으리라 예상했으나, 코드 상에선 이벤트 내용 텍스트를
bag of words 방식으로 분석한 뒤 CDN에서 해당 키워드의 이미지를 동적으로 받아오는
식으로 되어 있있다. dp는 mdpi에서 xxhdpi까지만 있음[^2].

- https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/
- https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/hdpi/
- https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/xhdpi/
- https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/xxhdpi/

예시:

- https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_hiking.jpg
- https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/xxhdpi/img_lunch.jpg

[^1]: https://www.reddit.com/r/google/comments/166rtrh/google_calendar_banners_i_have_found/
[^2]: https://developer.android.com/training/multiscreen/screendensities

## 참고
- https://github.com/mifran/google-calendar-image-keyword

## 과정
구글 켈린더 apk를 AVD 혹은 픽셀 폰에서 빼오기.

기기상의 APK 경로 확인
```sh
adb shell pm path com.google.android.calendar
```

APK 호스트로 다운
```sh
adb pull /system/priv-app/CalendarProvider/CalendarProvider.apk
```

압축 해제
```sh
mkdir CalendarProvider
cd CalendarProvider
unzip ../CalendarProvider.apk
```

Protobuf 파일에서 문자열 추출해 보기. Hack: 영어 버전보다는 한국어 버전을 쓰면
다른 영단어 키워드가 나오지 않아 편함.

```sh
strings ./assets/flairs/flairdata_ko.pb
```

다운받기

```sh
for res in mdpi hdpi xhdpi xxhdpi
do
	mkdir "$res"
	cd "$res"

	strings ./assets/flairs/flairdata_ko.pb |
		sed -E -e 's/^\s+//' -e 's/\s+$//' -e 's/ /-/g' |
		sort |
		uniq |
		while read f
		do
			wget "https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/$res/img_$f.jpg"
		done
done
```

## 목록
```
img_hiking.jpg
img_lunch.jpg
img_athleticsjumping.jpg
img_cricket.jpg
img_nowruz.jpg
img_repair.jpg
img_dancing.jpg
img_volleyball.jpg
img_quinceanera.jpg
img_rhythmicgymnastics.jpg
img_saintpatricksday.jpg
img_learnlanguage.jpg
img_xmasmeal.jpg
img_drinks.jpg
img_cycling.jpg
img_pride.jpg
img_running.jpg
img_code.jpg
img_halloween.jpg
img_boxing.jpg
img_sailing.jpg
img_xmas.jpg
img_rowing.jpg
img_walkingdog.jpg
img_read.jpg
img_shooting.jpg
img_bookclub.jpg
img_manicure.jpg
img_wedding.jpg
img_climbing.jpg
img_cooking.jpg
img_theatreopera.jpg
img_planmyday.jpg
img_gamenight.jpg
img_americanfootball.jpg
img_kayaking.jpg
img_graduation.jpg
img_valentinesday.jpg
img_waterpolo.jpg
img_fencing.jpg
img_massage.jpg
img_violin2.jpg
img_camping.jpg
img_reachout.jpg
img_artisticgymnastics.jpg
img_cinema.jpg
img_wrestling.jpg
img_archery.jpg
img_pingpong.jpg
img_santa.jpg
img_breakfast.jpg
img_sleep.jpg
img_golf.jpg
img_xmasparty.jpg
img_backtoschool.jpg
img_fieldhockey.jpg
img_bowling.jpg
img_karate.jpg
img_handcraft.jpg
img_athleticsthrowing.jpg
img_birthday.jpg
img_billiard.jpg
img_dentist.jpg
img_soccer.jpg
img_clean.jpg
img_datenight.jpg
img_beer.jpg
img_thanksgiving.jpg
img_learninstrument.jpg
img_babyshower.jpg
img_mardigras.jpg
img_handball.jpg
img_swimming.jpg
img_badminton.jpg
img_baseball.jpg
img_walk.jpg
img_genericnewyear.jpg
img_concert.jpg
img_carmaintenance.jpg
img_rugbysevens.jpg
img_haircut.jpg
img_equestrian.jpg
img_tennis.jpg
img_oilchange.jpg
img_basketball.jpg
img_dinner.jpg
img_skiing2.jpg
img_islamicnewyear.jpg
img_chinesenewyear.jpg
img_coffee.jpg
img_videogaming.jpg
img_yoga.jpg
img_triathlon.jpg
img_cyclingbmx.jpg
```

## 겔러리
![Flair image "athleticsjumping"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_athleticsjumping.jpg)
![Flair image "hiking"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_hiking.jpg)
![Flair image "lunch"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_lunch.jpg)
![Flair image "cricket"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_cricket.jpg)
![Flair image "nowruz"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_nowruz.jpg)
![Flair image "repair"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_repair.jpg)
![Flair image "dancing"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_dancing.jpg)
![Flair image "volleyball"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_volleyball.jpg)
![Flair image "quinceanera"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_quinceanera.jpg)
![Flair image "rhythmicgymnastics"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_rhythmicgymnastics.jpg)
![Flair image "saintpatricksday"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_saintpatricksday.jpg)
![Flair image "learnlanguage"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_learnlanguage.jpg)
![Flair image "xmasmeal"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_xmasmeal.jpg)
![Flair image "drinks"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_drinks.jpg)
![Flair image "cycling"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_cycling.jpg)
![Flair image "pride"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_pride.jpg)
![Flair image "running"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_running.jpg)
![Flair image "code"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_code.jpg)
![Flair image "halloween"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_halloween.jpg)
![Flair image "boxing"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_boxing.jpg)
![Flair image "sailing"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_sailing.jpg)
![Flair image "xmas"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_xmas.jpg)
![Flair image "rowing"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_rowing.jpg)
![Flair image "walkingdog"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_walkingdog.jpg)
![Flair image "read"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_read.jpg)
![Flair image "shooting"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_shooting.jpg)
![Flair image "bookclub"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_bookclub.jpg)
![Flair image "manicure"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_manicure.jpg)
![Flair image "wedding"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_wedding.jpg)
![Flair image "climbing"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_climbing.jpg)
![Flair image "cooking"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_cooking.jpg)
![Flair image "theatreopera"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_theatreopera.jpg)
![Flair image "planmyday"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_planmyday.jpg)
![Flair image "gamenight"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_gamenight.jpg)
![Flair image "americanfootball"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_americanfootball.jpg)
![Flair image "kayaking"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_kayaking.jpg)
![Flair image "graduation"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_graduation.jpg)
![Flair image "valentinesday"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_valentinesday.jpg)
![Flair image "waterpolo"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_waterpolo.jpg)
![Flair image "fencing"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_fencing.jpg)
![Flair image "massage"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_massage.jpg)
![Flair image "violin2"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_violin2.jpg)
![Flair image "camping"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_camping.jpg)
![Flair image "reachout"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_reachout.jpg)
![Flair image "artisticgymnastics"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_artisticgymnastics.jpg)
![Flair image "cinema"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_cinema.jpg)
![Flair image "wrestling"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_wrestling.jpg)
![Flair image "archery"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_archery.jpg)
![Flair image "pingpong"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_pingpong.jpg)
![Flair image "santa"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_santa.jpg)
![Flair image "breakfast"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_breakfast.jpg)
![Flair image "sleep"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_sleep.jpg)
![Flair image "golf"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_golf.jpg)
![Flair image "xmasparty"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_xmasparty.jpg)
![Flair image "backtoschool"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_backtoschool.jpg)
![Flair image "fieldhockey"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_fieldhockey.jpg)
![Flair image "bowling"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_bowling.jpg)
![Flair image "karate"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_karate.jpg)
![Flair image "handcraft"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_handcraft.jpg)
![Flair image "athleticsthrowing"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_athleticsthrowing.jpg)
![Flair image "birthday"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_birthday.jpg)
![Flair image "billiard"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_billiard.jpg)
![Flair image "dentist"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_dentist.jpg)
![Flair image "soccer"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_soccer.jpg)
![Flair image "clean"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_clean.jpg)
![Flair image "datenight"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_datenight.jpg)
![Flair image "beer"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_beer.jpg)
![Flair image "thanksgiving"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_thanksgiving.jpg)
![Flair image "learninstrument"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_learninstrument.jpg)
![Flair image "babyshower"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_babyshower.jpg)
![Flair image "mardigras"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_mardigras.jpg)
![Flair image "handball"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_handball.jpg)
![Flair image "swimming"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_swimming.jpg)
![Flair image "badminton"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_badminton.jpg)
![Flair image "baseball"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_baseball.jpg)
![Flair image "walk"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_walk.jpg)
![Flair image "genericnewyear"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_genericnewyear.jpg)
![Flair image "concert"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_concert.jpg)
![Flair image "carmaintenance"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_carmaintenance.jpg)
![Flair image "rugbysevens"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_rugbysevens.jpg)
![Flair image "haircut"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_haircut.jpg)
![Flair image "equestrian"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_equestrian.jpg)
![Flair image "tennis"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_tennis.jpg)
![Flair image "oilchange"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_oilchange.jpg)
![Flair image "basketball"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_basketball.jpg)
![Flair image "dinner"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_dinner.jpg)
![Flair image "skiing2"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_skiing2.jpg)
![Flair image "islamicnewyear"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_islamicnewyear.jpg)
![Flair image "chinesenewyear"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_chinesenewyear.jpg)
![Flair image "coffee"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_coffee.jpg)
![Flair image "videogaming"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_videogaming.jpg)
![Flair image "yoga"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_yoga.jpg)
![Flair image "triathlon"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_triathlon.jpg)
![Flair image "cyclingbmx"](https://ssl.gstatic.com/tmly/f8944938hffheth4ew890ht4i8/flairs/mdpi/img_cyclingbmx.jpg)
