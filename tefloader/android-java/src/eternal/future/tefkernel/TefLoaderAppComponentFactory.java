package eternal.future.tefkernel;

import android.annotation.SuppressLint;
import android.app.AppComponentFactory;
import android.content.Context;
import android.util.Log;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

/*******************************************************************************
 * TEFManager - TefLoaderAppComponentFactory
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
    static {
        Tefloader.initTefKernel(createAppContext());
    }

    public static Context createAppContext() {
        try {
            @SuppressLint("PrivateApi") Class<?> activityThreadClass = Class.forName("android.app.ActivityThread");
            @SuppressLint("PrivateApi") Method currentActivityThreadMethod = activityThreadClass.getDeclaredMethod("currentActivityThread");
            currentActivityThreadMethod.setAccessible(true);
            Object activityThread = currentActivityThreadMethod.invoke(null);

            @SuppressLint("PrivateApi") Field mBoundApplicationField = activityThreadClass.getDeclaredField("mBoundApplication");
            mBoundApplicationField.setAccessible(true);
            Object mBoundApplication = mBoundApplicationField.get(activityThread);

            @SuppressLint("PrivateApi") Class<?> appBindDataClass = Class.forName("android.app.ActivityThread$AppBindData");
            @SuppressLint("PrivateApi") Field infoField = appBindDataClass.getDeclaredField("info");
            infoField.setAccessible(true);
            Object loadedApk = infoField.get(mBoundApplication);

            @SuppressLint("PrivateApi") Class<?> contextImplClass = Class.forName("android.app.ContextImpl");
            assert loadedApk != null;
            @SuppressLint("PrivateApi") Method createAppContextMethod = contextImplClass.getDeclaredMethod(
                    "createAppContext",
                    activityThreadClass,
                    loadedApk.getClass()
            );
            createAppContextMethod.setAccessible(true);

            Object context = createAppContextMethod.invoke(null, activityThread, loadedApk);
            return (Context) context;
        } catch (Exception e) {
            Log.e("TefLoader", e.toString());
            return null;
        }
    }
}
