package com.nazatric.edgeoftime;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.util.TypedValue;
import android.view.Gravity;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Semi-transparent status banner drawn over the game surface.
 *
 * <p>The native launcher reports its lifecycle (Vulkan device creation, missing
 * game files, errors) through {@link NativeBridge#onNativeStatus(String)}; this
 * view renders it. It is deliberately simple text - the game itself renders
 * into the SDL surface underneath.
 */
public final class StatusOverlay {
    private final LinearLayout root;
    private final TextView statusText;

    public StatusOverlay(Context context) {
        root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.parseColor("#B0000000"));
        int pad = dp(context, 10);
        root.setPadding(pad, pad, pad, pad);

        TextView title = new TextView(context);
        title.setText("EDGE OF TIME - ANDROID PORT");
        title.setTextColor(Color.parseColor("#FFE8862D"));
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        root.addView(title);

        statusText = new TextView(context);
        statusText.setText(R.string.status_booting);
        statusText.setTextColor(Color.WHITE);
        statusText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        statusText.setTypeface(Typeface.MONOSPACE);
        root.addView(statusText);

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.TOP | Gravity.CENTER_HORIZONTAL);
        lp.topMargin = dp(context, 24);
        root.setLayoutParams(lp);
    }

    public LinearLayout getView() {
        return root;
    }

    public void setStatus(final String message) {
        statusText.post(() -> statusText.setText(message));
    }

    private static int dp(Context context, int value) {
        return (int) (value * context.getResources().getDisplayMetrics().density + 0.5f);
    }
}
