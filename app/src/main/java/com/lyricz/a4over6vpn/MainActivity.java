package com.lyricz.a4over6vpn;

import android.annotation.SuppressLint;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.VpnService;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.TextView;
import android.widget.Toast;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import java.util.Timer;
import java.util.TimerTask;

// MainActivity is just used for updating UI
public class MainActivity extends AppCompatActivity {

    // UI handler
    private static Handler ui = new Handler(Looper.getMainLooper());

    // UI components
    private TextView statisticsView;
    public static String UI_FILTER = "UI_CHANGE";
    public static String UI_STATUS = "UI_STATUS";

    // Parameters
    static int VPN_INTENT_REQUEST = 0;
    static String TAG = "MainActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Link UI components
        statisticsView = findViewById(R.id.statistics);

        // UI broadcast receiver
        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                statisticsView.setText(intent.getStringExtra(UI_STATUS));
            }
        };
        IntentFilter filter = new IntentFilter();
        filter.addAction(UI_FILTER);
        registerReceiver(receiver, filter);

        // For debug
        startVPNService();
    }

    // Press button and connect
    protected void startVPNService() {
        Intent intent = VpnService.prepare(MainActivity.this);
        startActivityForResult(intent, VPN_INTENT_REQUEST);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (requestCode == VPN_INTENT_REQUEST) {
            Intent intent = new Intent(this, VPNService.class);
            Log.i(TAG, "System VPN service begins running");
            startService(intent);
        } else {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    // Terminate service
    protected void stopVPNService() {
        Intent intent = new Intent(this, VPNService.class);
        stopService(intent);
        statisticsView.setText(R.string.data_statistics_bar);
    }

    @Override
    protected void onDestroy() {
        stopVPNService();
        super.onDestroy();
    }
}
