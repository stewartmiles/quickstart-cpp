// Copyright 2016 Google Inc. All Rights Reserved.

package com.google.firebase.example;

import android.app.Activity;
import android.app.NativeActivity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;

import android.util.Log;  // DEBUG

/**
 * Initialize Firebase before native activity calls the entry point.
 */
public class TestappNativeActivity extends NativeActivity {
  static {
    System.loadLibrary("android_main");
  }

  private static final String TAG = "TestappNativeActivity";

  private void displayFocus(String method) {
    Log.v(TAG, method + "(), Window: " + getWindow() + " Focus: " +
          hasWindowFocus());
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    displayFocus("onCreate");
    // Initialize Firebase.  Initialization is performed on a different thread
    // to allow for UI operations to proceed.
    final Activity activity = this;
    new Handler().post(new Runnable() {
            @Override
            public void run() {
                nativeInit(activity);
            }
        });
  }

  @Override
  protected void onStart() {
    super.onStart();
    displayFocus("onStart");
  }

  @Override
  public void onWindowFocusChanged(boolean hasFocus) {
   displayFocus("onWindowFocusChanged");
   if (hasFocus) {
     // This will enter native_app_glue which spawns the C++ main thread.
     // The main thread will block if INIT_IN_ACTIVITY_ONCREATE is 1
     // in android_main.cc, waiting until nativeOnCreate() is complete before
     // executing.
     nativeInit(this);  // Initialize Firebase.
   }
  }

  @Override
  protected void onResume() {
    super.onResume();
    displayFocus("onResume");
  }

  private native void nativeInit(Activity activity);
}
