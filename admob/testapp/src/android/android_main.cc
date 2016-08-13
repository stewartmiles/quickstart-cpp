// Copyright 2016 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdarg.h>
#include <stdio.h>

#include <android/log.h>
#include <android_native_app_glue.h>
#include <pthread.h>
#include <cassert>
#include <errno.h>
#include <jni.h>
#include <time.h>
#include <unistd.h>

#include "main.h"  // NOLINT

#define INIT_IN_ACTIVITY_ON_CREATE 1
#define WAIT_FOR_ACTIVITY_FOCUS 0

// This implementation is derived from http://github.com/google/fplutil

extern "C" int common_main(int argc, const char* argv[]);

static struct android_app* g_app_state = nullptr;
static bool g_destroy_requested = false;
static bool g_started = false;
static bool g_restarted = false;
static pthread_mutex_t g_started_mutex;
static JNIEnv* g_jni_env = nullptr;
static jobject g_activity = nullptr;

#if INIT_IN_ACTIVITY_ON_CREATE
static pthread_cond_t g_init_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_main_thread = 0;
static bool g_initialized = false;
#endif  // INIT_IN_ACTIVITY_ON_CREATE

extern void InitializeFirebase();
void CheckJNIException();

extern "C" JNIEXPORT void JNICALL
Java_com_google_firebase_example_TestappNativeActivity_nativeInit(
    JNIEnv *env, jobject thiz, jobject activity) {
#if INIT_IN_ACTIVITY_ON_CREATE
  LogMessage("Early init called.");
  // Initialize Firebase.
  g_jni_env = env;
  g_activity = activity;
  // NOTE: It's not possible to execute UI methods yet as no window exists.
  InitializeFirebase();
  pthread_mutex_lock(&g_init_mutex);
  g_initialized = true;
  pthread_mutex_unlock(&g_init_mutex);
  g_jni_env = nullptr;
  g_activity = nullptr;
  LogMessage("Early init complete");
  pthread_cond_signal(&g_init_cond);
#endif  // INIT_IN_ACTIVITY_ON_CREATE
}

// Handle state changes from via native app glue.
static void OnAppCmd(struct android_app* app, int32_t cmd) {
  g_destroy_requested |= cmd == APP_CMD_DESTROY;
}

// Process events pending on the main thread.
// Returns true when the app receives an event requesting exit.
bool ProcessEvents(int msec) {
#if INIT_IN_ACTIVITY_ON_CREATE
  if (!pthread_equal(g_main_thread, pthread_self())) {
    usleep(1000 * msec);
    return g_destroy_requested | g_restarted;
  }
#endif  // INIT_IN_ACTIVITY_ON_CREATE
  struct android_poll_source* source = nullptr;
  int events;
  int looperId = ALooper_pollAll(msec, nullptr, &events,
                                 reinterpret_cast<void**>(&source));
  if (looperId >= 0 && source) {
    source->process(g_app_state, source);
  }
  return g_destroy_requested | g_restarted;
}

// Get the activity.
jobject GetActivity() {
#if INIT_IN_ACTIVITY_ON_CREATE
  if (!pthread_equal(g_main_thread, pthread_self())) return g_activity;
#endif  // INIT_IN_ACTIVITY_ON_CREATE
  return g_app_state->activity->clazz;
}

// Get the window context. For Android, it's a jobject pointing to the Activity.
jobject GetWindowContext() { return GetActivity(); }

// Find a class, attempting to load the class if it's not found.
jclass FindClass(JNIEnv* env, jobject activity_object, const char* class_name) {
  jclass class_object = env->FindClass(class_name);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    // If the class isn't found it's possible NativeActivity is being used by
    // the application which means the class path is set to only load system
    // classes.  The following falls back to loading the class using the
    // Activity before retrieving a reference to it.
    jclass activity_class = env->FindClass("android/app/Activity");
    assert(activity_class);
    jmethodID activity_get_class_loader = env->GetMethodID(
        activity_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
    assert(activity_get_class_loader);

    jobject class_loader_object =
        env->CallObjectMethod(activity_object, activity_get_class_loader);
    assert(class_loader_object);

    jclass class_loader_class = env->FindClass("java/lang/ClassLoader");
    assert(class_loader_class);
    jmethodID class_loader_load_class =
        env->GetMethodID(class_loader_class, "loadClass",
                         "(Ljava/lang/String;)Ljava/lang/Class;");
    assert(class_loader_load_class);
    jstring class_name_object = env->NewStringUTF(class_name);
    assert(class_name_object);

    class_object = static_cast<jclass>(env->CallObjectMethod(
        class_loader_object, class_loader_load_class, class_name_object));
    assert(class_object);

    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      class_object = nullptr;
    }
    env->DeleteLocalRef(class_name_object);
    env->DeleteLocalRef(class_loader_object);
  }
  return class_object;
}

// Vars that we need available for appending text to the log window:
class LoggingUtilsData {
 public:
  LoggingUtilsData()
      : logging_utils_class_(nullptr),
        logging_utils_add_log_text_(0),
        logging_utils_init_log_window_(0) {}

  ~LoggingUtilsData() {
    JNIEnv* env = GetJniEnv();
    assert(env);
    if (logging_utils_class_) {
      env->DeleteGlobalRef(logging_utils_class_);
    }
  }

  void Init() {
    LogMessage("get env");
    JNIEnv* env = GetJniEnv();
    assert(env);

    LogMessage("find logging utils");
    jclass logging_utils_class = FindClass(
        env, GetActivity(), "com/google/firebase/example/LoggingUtils");
    CheckJNIException();
    assert(logging_utils_class != 0);

    LogMessage("reference class");
    // Need to store as global references so it don't get moved during garbage
    // collection.
    logging_utils_class_ =
        static_cast<jclass>(env->NewGlobalRef(logging_utils_class));
    env->DeleteLocalRef(logging_utils_class);

    LogMessage("get init log window");
    logging_utils_init_log_window_ = env->GetStaticMethodID(
        logging_utils_class_, "initLogWindow", "(Landroid/app/Activity;)V");
    LogMessage("get add log text");
    logging_utils_add_log_text_ = env->GetStaticMethodID(
        logging_utils_class_, "addLogText", "(Ljava/lang/String;)V");

    LogMessage("call init");
    env->CallStaticVoidMethod(logging_utils_class_,
                              logging_utils_init_log_window_, GetActivity());
  }

  void AppendText(const char* text) {
    if (logging_utils_class_ == 0) return;  // haven't been initted yet
    JNIEnv* env = GetJniEnv();
    assert(env);
    jstring text_string = env->NewStringUTF(text);
    env->CallStaticVoidMethod(logging_utils_class_, logging_utils_add_log_text_,
                              text_string);
    env->DeleteLocalRef(text_string);
  }

 private:
  jclass logging_utils_class_;
  jmethodID logging_utils_add_log_text_;
  jmethodID logging_utils_init_log_window_;
};

LoggingUtilsData* g_logging_utils_data = nullptr;

// Checks if a JNI exception has happened, and if so, logs it to the console.
void CheckJNIException() {
  JNIEnv* env = GetJniEnv();
  if (env->ExceptionCheck()) {
    // Get the exception text.
    jthrowable exception = env->ExceptionOccurred();
    env->ExceptionClear();

    // Convert the exception to a string.
    jclass object_class = env->FindClass("java/lang/Object");
    jmethodID toString =
        env->GetMethodID(object_class, "toString", "()Ljava/lang/String;");
    jstring s = (jstring)env->CallObjectMethod(exception, toString);
    const char* exception_text = env->GetStringUTFChars(s, nullptr);

    // Log the exception text.
    __android_log_print(ANDROID_LOG_INFO, FIREBASE_TESTAPP_NAME,
                        "-------------------JNI exception:");
    __android_log_print(ANDROID_LOG_INFO, FIREBASE_TESTAPP_NAME, "%s",
                        exception_text);
    __android_log_print(ANDROID_LOG_INFO, FIREBASE_TESTAPP_NAME,
                        "-------------------");

    // Also, assert fail.
    assert(false);

    // In the event we didn't assert fail, clean up.
    env->ReleaseStringUTFChars(s, exception_text);
    env->DeleteLocalRef(s);
    env->DeleteLocalRef(exception);
  }
}

// Log a message that can be viewed in "adb logcat".
void LogMessage(const char* format, ...) {
  static const int kLineBufferSize = 100;
  char buffer[kLineBufferSize + 2];

  va_list list;
  va_start(list, format);
  int string_len = vsnprintf(buffer, kLineBufferSize, format, list);
  string_len = string_len < kLineBufferSize ? string_len : kLineBufferSize;
  // append a linebreak to the buffer:
  buffer[string_len] = '\n';
  buffer[string_len + 1] = '\0';

  __android_log_vprint(ANDROID_LOG_INFO, FIREBASE_TESTAPP_NAME, format, list);
#if INIT_IN_ACTIVITY_ON_CREATE
  if (pthread_equal(g_main_thread, pthread_self())) {
#endif  // INIT_IN_ACTIVITY_ON_CREATE
  if (g_logging_utils_data) {
    g_logging_utils_data->AppendText(buffer);
    CheckJNIException();
  }
#if INIT_IN_ACTIVITY_ON_CREATE
  }
#endif  // INIT_IN_ACTIVITY_ON_CREATE
  va_end(list);
}

// Get the JNI environment.
JNIEnv* GetJniEnv() {
#if INIT_IN_ACTIVITY_ON_CREATE
  if (!pthread_equal(g_main_thread, pthread_self())) return g_jni_env;
#endif  // INIT_IN_ACTIVITY_ON_CREATE
  JavaVM* vm = g_app_state->activity->vm;
  JNIEnv* env;
  jint result = vm->AttachCurrentThread(&env, nullptr);
  return result == JNI_OK ? env : nullptr;
}

// Execute common_main(), flush pending events and finish the activity.
extern "C" void android_main(struct android_app* state) {
  // native_app_glue spawns a new thread, calling android_main() when the
  // activity onStart() or onRestart() methods are called.  This code handles
  // the case where we're re-entering this method on a different thread by
  // signalling the existing thread to exit, waiting for it to complete before
  // reinitializing the application.
  if (g_started) {
    g_restarted = true;
    // Wait for the existing thread to exit.
    pthread_mutex_lock(&g_started_mutex);
    pthread_mutex_unlock(&g_started_mutex);
  } else {
    g_started_mutex = PTHREAD_MUTEX_INITIALIZER;
  }
  pthread_mutex_lock(&g_started_mutex);
#if INIT_IN_ACTIVITY_ON_CREATE
  g_main_thread = pthread_self();
#endif  // INIT_IN_ACTIVITY_ON_CREATE
  g_started = true;

  // Save native app glue state and setup a callback to track the state.
  g_destroy_requested = false;
  g_app_state = state;
  g_app_state->onAppCmd = OnAppCmd;

  // Create the logging display.
  {
    LoggingUtilsData* logging_utils_data = new LoggingUtilsData();
    logging_utils_data->Init();
    g_logging_utils_data = logging_utils_data;
  }

#if INIT_IN_ACTIVITY_ON_CREATE
  LogMessage("Logging display up");

  // Wait for nativeInit() to complete.
  pthread_mutex_lock(&g_init_mutex);
  struct timespec time_to_wait_for;
  while (!g_initialized) {
    if (ProcessEvents(10)) {
      break;
    }
    // Wait for the condition with a timeout of 1ms.
    static const int64_t nsec_per_sec = 1000000000;
    static const int64_t nsec_per_ms = nsec_per_sec / 1000;
    clock_gettime(CLOCK_REALTIME, &time_to_wait_for);
    int64_t nsec = time_to_wait_for.tv_nsec;
    nsec += nsec_per_ms * 10;
    time_to_wait_for.tv_nsec = nsec % nsec_per_sec;
    time_to_wait_for.tv_sec += nsec / nsec_per_sec;
    pthread_cond_timedwait(&g_init_cond, &g_init_mutex, &time_to_wait_for);
  }
  pthread_mutex_unlock(&g_init_mutex);

#endif  // INIT_IN_ACTIVITY_ON_CREATE
#if WAIT_FOR_ACTIVITY_FOCUS
  // Wait for the window to gain focus.
  // This is required as we can't show an Ad (it's a popup window) until the
  // window is in focus.
  {
    JNIEnv *env = GetJniEnv();
    jclass activity_class = env->FindClass("android/app/Activity");
    assert(activity_class);
    jmethodID activity_has_window_focus = env->GetMethodID(
      activity_class, "hasWindowFocus", "()Z");
    assert(activity_has_window_focus);
    while (!env->CallBooleanMethod(GetActivity(), activity_has_window_focus) &&
           !ProcessEvents(10)) {
    }
  }
#endif  // WAIT_FOR_ACTIVITY_FOCUS

  // Execute cross platform entry point.
  static const char* argv[] = {FIREBASE_TESTAPP_NAME};
  int return_value = common_main(1, argv);
  (void)return_value;  // Ignore the return value.
  ProcessEvents(10);

  // Clean up logging display.
  delete g_logging_utils_data;
  g_logging_utils_data = nullptr;

#if INIT_IN_ACTIVITY_ON_CREATE
  g_initialized = false;
#endif  // INIT_IN_ACTIVITY_ON_CREATE

  // Finish the activity.
  if (!g_restarted) ANativeActivity_finish(state->activity);

  g_app_state->activity->vm->DetachCurrentThread();
  g_started = false;
  g_restarted = false;
  pthread_mutex_unlock(&g_started_mutex);
}
