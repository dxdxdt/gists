# Everything React Native
I decided to take up on a hobby/portfolio project of a random bloke on the
internet. What he had in mind is basically a mobile app with basic chat
features. I suggested we should build it on React Native(RN) as I assumed it is

- Used by many corporations so it must be feature-rich
- Maintained for over a decade so it must have matured
- Their "Write once, run anywhere"(WORA) motto should work
- I did web before. I know JS. So RN should be easy!

I was horribly wrong. The more I faced the issues of RN that don't exist when
developing native apps, the more I realise that all of the assumptions were just
wishful thinking. I fell right into the good olde fallacy of lazy programmers.
RN turned out to be a beast born out of "design by incompetent committee" and
pure negligence. Everything is a plugin. To make the matter worse, the core devs
make no attempt to embrace some of the widely used plugins to the core API.
Interfaces are broken all the time(typical in the JS ecosystem). A hugh amount
of plugins are pumped out and dumped away. Seriously, keep your web dev
ecosystem problems in the web. No need to bring that BS to the mobile app dev.

RN is such a mess and here are the reasons why I think it is. I know that all
the complains about RN are basically first-world problems. I can always ditch RN
start building the apps from the scratch. As an experienced Android dev, this
could have been the right choice. Or using RN would eventually yield good
productivity. I don't know. But I decided to bet my money on the WORA part of
RN. We simply don't have the resources to make native apps. Java has the same
issue and Java devs complain about it all the time. That doesn't make them ditch
Java and move to another framework/language because Java still gets shit done.
Hopefully, the same logic can be applied to RN here, too.

The list starts here.

## System Color Scheme
Take a look at how RN achieves the dark mode.

```diff
diff --git a/App.tsx b/App.tsx
index 125fe1b..e5a2d40 100644
--- a/App.tsx
+++ b/App.tsx
@@ -13,12 +13,10 @@ import {
   StatusBar,
   StyleSheet,
   Text,
-  useColorScheme,
   View,
 } from 'react-native';

 import {
-  Colors,
   DebugInstructions,
   Header,
   LearnMoreLinks,
@@ -30,25 +28,12 @@ type SectionProps = PropsWithChildren<{
 }>;

 function Section({children, title}: SectionProps): React.JSX.Element {
-  const isDarkMode = useColorScheme() === 'dark';
   return (
     <View style={styles.sectionContainer}>
-      <Text
-        style={[
-          styles.sectionTitle,
-          {
-            color: isDarkMode ? Colors.white : Colors.black,
-          },
-        ]}>
+      <Text>
         {title}
       </Text>
-      <Text
-        style={[
-          styles.sectionDescription,
-          {
-            color: isDarkMode ? Colors.light : Colors.dark,
-          },
-        ]}>
+      <Text>
         {children}
       </Text>
     </View>
@@ -56,26 +41,14 @@ function Section({children, title}: SectionProps): React.JSX.Element {
 }

 function App(): React.JSX.Element {
-  const isDarkMode = useColorScheme() === 'dark';
-
-  const backgroundStyle = {
-    backgroundColor: isDarkMode ? Colors.darker : Colors.lighter,
-  };
-
   return (
-    <SafeAreaView style={backgroundStyle}>
+    <SafeAreaView>
       <StatusBar
-        barStyle={isDarkMode ? 'light-content' : 'dark-content'}
-        backgroundColor={backgroundStyle.backgroundColor}
       />
       <ScrollView
-        contentInsetAdjustmentBehavior="automatic"
-        style={backgroundStyle}>
+        contentInsetAdjustmentBehavior="automatic">
         <Header />
-        <View
-          style={{
-            backgroundColor: isDarkMode ? Colors.black : Colors.white,
-          }}>
+        <View>
           <Section title="Step One">
             Edit <Text style={styles.highlight}>App.tsx</Text> to change this
             screen and then come back to see your edits.
```

[WTF is this
POS?](https://knowyourmeme.com/memes/what-the-fuck-is-this-piece-of-shit)

So many lines of code just to respond dynamically to the color scheme change.
This definitely adds too much of burden to the dev. If you run the version b and
switch to the dark theme using the Quick Settings, you'll find that the app does
not bode well with the change.

![RN app when dark theme switched](dark-theme-switch.gif)

Why is that even necessary? For a native Android app that does not define any
style, the system takes care of the change for you. The problem is just old apps
written before the concept of dark themes that mess around with the colors of
components. That is exactly the problem with RN. It plays around with the style.

Then I thought: is this just a limitation of web apps? So I went on to see how
many websites respond to the color scheme change. Here are some of the websites
I found:

- Reddit
- Google News
- Github
- Youtube(when refreshed)
- X

It's made possible by [CSS's media
query](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-color-scheme)
and the browser engine listening to the change event and elegantly propagating
it all the way to the layout engine. There's no excuse for RN to not catching
on.

## I18n and L10n
It's 2024 and RN doesn't come with i18n and l10n. Android started with the solid
build-in `getString()` from the get-go. Xcode offers something similar. RN
didn't just care. So devs have to resort to plugins that offers no guarantee
that they will work in 2 years time. Yes, RN is a UI framework that does not
come with the basic UI framework.

## No Asset System
Packaging arbitrary data and ability to load it up at runtime is non-existent.
[The doc](https://reactnative.dev/docs/images#static-non-image-resources) says
it is possible to bundle files like pdf and html... Without elaborating on how
you'd use them in the code at all. Because you can't. In the unified way anyway.
The doc is obviously misleading. If you have read the docs with a grain of salt,
you know you're in trouble. I'm not the first and only. Many people have pointed
this out before.

- https://github.com/react-native-webview/react-native-webview/issues/428
- https://github.com/react-native-webview/react-native-webview/issues/518
- https://github.com/react-native-webview/react-native-webview/issues/2760

A quick googling will tell you that devs just make do with **react-native-fs**.
Yes, throw another plugin to the project that's already messy with all the
plugins that have to compiled and bundled up. And the plugin is not a good
solution to the problem because it says "use this for Android", "use this for
IOS", and "you can't do that on IOS". So much for WORA, huh.

The way `require()` is written for RN is terrible. The function is not clearly
documented anywhere. It behaves like a synchronous synonym to `import` for JS
modules. But RN encourages devs to use it to package media files like images and
videos. When used to "import" image files, the function returns a `number`, on
which the core components use `Image.resolveAssetSource()` to get the
"uri"(which in fact is actually a filename in the drawable directory) to load
and render. So, the return type is transparent for JS modules(you can use it
directly) and opaque for assets. `Image.resolveAssetSource()` shouldn't be
exposed in the first place because there's nothing devs can do with the uri
returned from it.

And `require()`'s param must be a string literal so that Metro scan and bundle
the assets? And the function is meant to do "dynamic" loading? It's confusing
the hell out of me. Well, many other platforms provide resource system so you
can manage assets yourself and this is the very reason why.
