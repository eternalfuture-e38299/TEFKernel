package eternal.future.tefkernel;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AppComponentFactory;
import android.app.Application;
import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

/*******************************************************************************
 * TEFKernel - TefLoaderAppComponentFactory
 * Copyright (C) 2026 eternalfuture-e38299
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2026/2/28
 *******************************************************************************/

public class TefLoaderAppComponentFactory extends AppComponentFactory {
    private static volatile boolean sInitialized = false;
    private static final Object sLock = new Object();

    @Override
    public Application instantiateApplication(ClassLoader cl, String className)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        // 让父类先创建 Application
        Application app = super.instantiateApplication(cl, className);

        // 延迟初始化 TefKernel
        initTefKernelDelayed(app);

        return app;
    }

    @Override
    public Activity instantiateActivity(ClassLoader cl, String className, Intent intent)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        // 在第一个 Activity 实例化时进行初始化
        initTefKernelIfNeeded();
        return super.instantiateActivity(cl, className, intent);
    }

    @Override
    public android.app.Service instantiateService(ClassLoader cl, String className, Intent intent)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        initTefKernelIfNeeded();
        return super.instantiateService(cl, className, intent);
    }

    @Override
    public android.content.ContentProvider instantiateProvider(ClassLoader cl, String className)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        // ContentProvider 可能是最早初始化的组件
        android.content.ContentProvider provider = super.instantiateProvider(cl, className);
        initTefKernelIfNeeded();
        return provider;
    }

    @Override
    public android.content.BroadcastReceiver instantiateReceiver(ClassLoader cl, String className, Intent intent)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        initTefKernelIfNeeded();
        return super.instantiateReceiver(cl, className, intent);
    }

    private void initTefKernelDelayed(Application app) {
        // 监听应用的 onCreate
        app.registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks() {
            @Override
            public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
                // 在第一个 Activity 创建时初始化
                initTefKernelIfNeeded();
                app.unregisterActivityLifecycleCallbacks(this);
            }

            @Override
            public void onActivityStarted(Activity activity) {}

            @Override
            public void onActivityResumed(Activity activity) {}

            @Override
            public void onActivityPaused(Activity activity) {}

            @Override
            public void onActivityStopped(Activity activity) {}

            @Override
            public void onActivitySaveInstanceState(Activity activity, Bundle outState) {}

            @Override
            public void onActivityDestroyed(Activity activity) {}
        });
    }

    private void initTefKernelIfNeeded() {
        if (!sInitialized) {
            synchronized (sLock) {
                if (!sInitialized) {
                    try {
                        Context context = getApplicationContext();
                        if (context != null) {
                            Tefloader.initTefKernel(context);
                            Log.d("TefLoader", "TEFKernel initialized successfully in AppComponentFactory");
                            sInitialized = true;
                        } else {
                            Log.w("TefLoader", "Context is null, delaying TEFKernel initialization");
                        }
                    } catch (Throwable e) {
                        Log.e("TefLoader", "Failed to initialize TEFKernel: " + e.getMessage(), e);
                    }
                }
            }
        }
    }

    @SuppressLint({"PrivateApi", "DiscouragedPrivateApi"})
    private static Context getApplicationContext() {
        try {
            // 方法1: 通过 ActivityThread
            Class<?> activityThreadClass = Class.forName("android.app.ActivityThread");
            Method currentActivityThreadMethod = activityThreadClass.getDeclaredMethod("currentActivityThread");
            currentActivityThreadMethod.setAccessible(true);
            Object activityThread = currentActivityThreadMethod.invoke(null);

            if (activityThread != null) {
                Method getApplicationMethod = activityThreadClass.getDeclaredMethod("getApplication");
                getApplicationMethod.setAccessible(true);
                Application app = (Application) getApplicationMethod.invoke(activityThread);
                if (app != null) {
                    return app.getApplicationContext();
                }
            }

            // 方法2: 通过反射获取 mInitialApplication
            if (activityThread != null) {
                Field mInitialApplicationField = activityThreadClass.getDeclaredField("mInitialApplication");
                mInitialApplicationField.setAccessible(true);
                Application initialApp = (Application) mInitialApplicationField.get(activityThread);
                if (initialApp != null) {
                    return initialApp.getApplicationContext();
                }
            }

            // 方法3: 尝试通过 LoadedApk
            Field mBoundApplicationField = activityThreadClass.getDeclaredField("mBoundApplication");
            mBoundApplicationField.setAccessible(true);
            Object mBoundApplication = mBoundApplicationField.get(activityThread);

            if (mBoundApplication != null) {
                Class<?> appBindDataClass = Class.forName("android.app.ActivityThread$AppBindData");
                Field infoField = appBindDataClass.getDeclaredField("info");
                infoField.setAccessible(true);
                Object loadedApk = infoField.get(mBoundApplication);

                if (loadedApk != null) {
                    // 通过 LoadedApk 获取 Context
                    Class<?> loadedApkClass = Class.forName("android.app.LoadedApk");
                    Method getResourcesMethod = loadedApkClass.getDeclaredMethod("getResources");
                    getResourcesMethod.setAccessible(true);
                    Object resources = getResourcesMethod.invoke(loadedApk);

                    if (resources instanceof Context) {
                        return (Context) resources;
                    }
                }
            }

        } catch (Exception e) {
            Log.e("TefLoader", "Failed to get application context: " + e.getMessage(), e);
        }

        return null;
    }
}
