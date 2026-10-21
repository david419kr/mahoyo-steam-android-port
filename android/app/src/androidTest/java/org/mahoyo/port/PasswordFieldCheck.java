package org.mahoyo.port;

import android.app.Instrumentation;
import android.content.Intent;
import android.text.method.PasswordTransformationMethod;
import android.view.*;
import android.widget.*;

/** Uses synthetic input only; never logs in or reads an account's credentials. */
final class PasswordFieldCheck {
    static String run(Instrumentation probe)throws Exception{
        InstallActivity activity=(InstallActivity)probe.startActivitySync(new Intent(probe.getTargetContext(),InstallActivity.class).putExtra("manage",true).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        probe.waitForIdleSync();android.os.SystemClock.sleep(150);probe.waitForIdleSync();
        java.util.concurrent.atomic.AtomicReference<Throwable> failure=new java.util.concurrent.atomic.AtomicReference<>();
        probe.runOnMainSync(()->{try{
            java.lang.reflect.Method form=InstallActivity.class.getDeclaredMethod("loginFormScreen");form.setAccessible(true);form.invoke(activity);
            java.lang.reflect.Field input=InstallActivity.class.getDeclaredField("password");input.setAccessible(true);EditText password=(EditText)input.get(activity);
            password.setText("synthetic-password-061");password.setSelection(4);
            if(!(password.getTransformationMethod() instanceof PasswordTransformationMethod)||password.isSaveEnabled())throw new AssertionError("Password is not masked safely by default");
            View eye=find(activity.getWindow().getDecorView(),activity.getString(R.string.setup_password_show));if(eye==null)throw new AssertionError("Password visibility button missing");
            eye.performClick();if(password.getTransformationMethod()!=null||password.getSelectionStart()!=4)throw new AssertionError("Reveal changed cursor or stayed masked");
            eye.performClick();if(!(password.getTransformationMethod() instanceof PasswordTransformationMethod)||password.getSelectionStart()!=4)throw new AssertionError("Hide changed cursor or failed");
            password.setText("");activity.finish();
        }catch(Throwable problem){failure.set(problem);}});
        if(failure.get()!=null)throw new AssertionError("Password UI regression",failure.get());
        return "Real Android password field defaults to masked, toggles reveal/hide, preserves cursor and disables saved view state. Synthetic input only.\n";
    }
    private static View find(View view,String description){
        if(description.contentEquals(view.getContentDescription()==null?"":view.getContentDescription()))return view;
        if(view instanceof ViewGroup){ViewGroup group=(ViewGroup)view;for(int i=0;i<group.getChildCount();i++){View found=find(group.getChildAt(i),description);if(found!=null)return found;}}
        return null;
    }
}
