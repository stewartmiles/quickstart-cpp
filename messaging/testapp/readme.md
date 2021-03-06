Firebase Cloud Messaging Quickstart
===================================

The Firebase Cloud Messaging Test Application (testapp) demonstrates receiving
Firebase Cloud Messages using the Firebase Cloud Messaging C++ SDK. This
application has no user interface and simply logs actions it's performing to the
console.

Introduction
------------

- [Read more about Firebase Cloud Messaging](https://firebase.google.com/docs/cloud-messaging/)

Building and Running the testapp
--------------------------------
### iOS
  - Link your iOS app to the Firebase libraries.
    - Get CocoaPods version 1 or later by running,

    ```
    $ sudo gem install CocoaPods --pre
    ```
    - From the testapp directory, install the CocoaPods listed in the Podfile by
    running,
    ```
    $ pod install
    ```
    - Open the generated Xcode workspace (which now has the CocoaPods),

    ```
    $ open testapp.xcworkspace
    ```
    - For further details please refer to the
    [general instructions for setting up an iOS app with Firebase](https://firebase.google.com/docs/ios/setup).

  - Register your iOS app with Firebase.
    - Create a new app on
    [firebase.google.com/console](https://firebase.google.com/console/)
    , and attach your iOS app to it.
      - For Messaging, you will need an App Store ID. Use something random such
        as 12345678."
      - You can use "com.google.ios.messaging.testapp" as the iOS Bundle ID
        while you're testing.
    - Add the GoogleService-Info.plist that you downloaded from Firebase
      console to the testapp root directory. This file identifies your iOS app
      to the Firebase backend.
  - Download the Firebase C++ SDK linked from
    [https://firebase.google.com/docs/cpp/setup]() and unzip it to a
    directory of your choice.
  - Add the following frameworks from the Firebase C++ SDK to the project:
    - frameworks/ios/universal/firebase.framework
    - frameworks/ios/universal/firebase_messaging.framework
    - You will need to either,
       1. Check "Copy items if needed" when adding the frameworks, or
       2. Add the framework path in "Framework Search Paths"
          - e.g. If you downloaded the Firebase C++ SDK to
            `/Users/me/firebase_cpp_sdk`,
            then you would add the path
            `/Users/me/firebase_cpp_sdk/frameworks/ios/universal`.
          - To add the path, in XCode, select your project in the project
            navigator, then select your target in the main window.
            Select the "Build Settings" tab, and click "All" to see all
            the build settings. Scroll down to "Search Paths", and add
            your path to "Framework Search Paths".
          - You need a valid
            [APNs](https://developer.apple.com/library/ios/documentation/NetworkingInternet/Conceptual/RemoteNotificationsPG/Chapters/ApplePushService.html)
            certificate. If you don't already have one, see
            [Provisioning APNs SSL Certificates.](https://firebase.google.com/docs/cloud-messaging/ios/certs)
  - In XCode, build & run the sample on an iOS device or simulator.
  - The testapp has no user interface. The output of the app can be viewed
    via the console.  In Xcode,  select
    `View --> Debug Area --> Activate Console` from the menu.

### Android
**Register your Android app with Firebase.**

- Create a new app on
[firebase.google.com/console](https://firebase.google.com/console/)
, and attach your Android app to it.
- You can use "com.google.android.messaging.testapp" as the Package Name
while you're testing.
- To [generate a SHA1](https://developers.google.com/android/guides/client-auth)
run this command (_Note: the default password is 'android'_):
    * Mac and Linux:

        ```
        keytool -exportcert -list -v -alias androiddebugkey -keystore ~/.android/debug.keystore
        ```
    * Windows:

        ```
        keytool -exportcert -list -v -alias androiddebugkey -keystore %USERPROFILE%\.android\debug.keystore
        ```
- If keytool reports that you do not have a debug.keystore, you can
[create one](http://developer.android.com/tools/publishing/app-signing.html#signing-manually)
with:

    ```
    keytool -genkey -v -keystore ~/.android/debug.keystore -storepass android -alias androiddebugkey -keypass android -dname "CN=Android Debug,O=Android,C=US"
    ```
- Add the `google-services.json` file that you downloaded from Firebase
  console to the root directory of testapp. This file identifies your
  Android app to the Firebase backend.
- For further details please refer to the
[general instructions for setting up an Android app with Firebase](https://firebase.google.com/docs/android/setup).

- Download the Firebase C++ SDK linked from
  [https://firebase.google.com/docs/cpp/setup]() and unzip it to a
  directory of your choice.

**Configure your SDK paths**

- Configure the location of the Firebase C++ SDK by setting the
`firebase_cpp_sdk.dir` Gradle property to the SDK install directory.
  * Run this command in the project directory, and modify the path to match your
  local installation path:

    ```
    > echo "systemProp.firebase_cpp_sdk.dir=/User/$USER/firebase_cpp_sdk" >> gradle.properties
    ```
- Ensure the Android SDK and NDK locations are set in Android Studio.
    * From the Android Studio launch menu, go to
    `Configure/Project Defaults/Project Structure` and download the SDK and NDK
    if the locations are not yet set.
    * Android Studio will write these paths to `local.properties`.

**Build & Run**

- Open `build.gradle` in Android Studio.
    - From the Android Studio launch menu, "Open an existing Android Studio
      project", and select `build.gradle`.
- Install the SDK Platforms that Android Studio reports missing.
- Build the testapp and run it on an Android device or emulator.
- See [below](#using_the_test_app) for usage instructions.

Using the Test App
------------------

- Install and run the test app on your iOS or Android device or emulator.
- The application has minimal user interface. The output of the app can be
viewed via the console:
    * **iOS**: Open select "View --> Debug Area --> Activate Console" from the
    menu in Xcode.
    * **Android**: View the logcat output in Android studio or by running
    "adb logcat" from the command line.
- When you first run the app, it will print:
`Recieved Registration Token: <code>`. Copy this code to a text editor.
- Copy the ServerKey from the firebase console:
  - Open your project in the
    [firebase console](https://firebase.google.com/console/)
  - Click `Notifications` in the menu on the left
  - Select the `Credentials` tab.
  - Copy the `Server Key`
- Replace `<Server Key>` and `<Registration Token>` in this command and run it
from the command line.
```
curl --header "Authorization: key=<Server Key>" --header "Content-Type: application/json" https://android.googleapis.com/gcm/send -d '{"notification":{"title":"Hi","body":"Hello from the Cloud"},"data":{"score":"lots"},"to":"<Registration Token>"}'
```
- Observe the command received in the app, via the console output.

Support
-------

[https://firebase.google.com/support/]()

License
-------

Copyright 2016 Google, Inc.

Licensed to the Apache Software Foundation (ASF) under one or more contributor
license agreements.  See the NOTICE file distributed with this work for
additional information regarding copyright ownership.  The ASF licenses this
file to you under the Apache License, Version 2.0 (the "License"); you may not
use this file except in compliance with the License.  You may obtain a copy of
the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
License for the specific language governing permissions and limitations under
the License.

