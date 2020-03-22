// Java UI of 4over6 VPN client
// 2020 Network Training, Tsinghua University
// Chenggang Zhao & Yuxian Gu

package com.lyricz.a4over6vpn;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.VpnService;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

// MainActivity is just used for updating UI
public class MainActivity extends AppCompatActivity {

    // UI components
    private TextView statisticsView;
    private EditText addrEdit, portEdit;
    private Button connectButton;
    public static String UI_FILTER = "UI_CHANGE";
    public static String UI_STATUS = "UI_STATUS";
    public static String UI_CREATE = "UI_CREATE";
    public static String UI_FAILED = "UI_FAILED";
    public static String UI_BREAK  = "UI_BREAK";

    // Intent Extra
    public static String INTENT_ADDR = "addr";
    public static String INTENT_PORT = "port";

    // Parameters
    static int VPN_INTENT_REQUEST = 0;
    static String TAG = "MainActivity";
    boolean connected = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Link UI components
        statisticsView = findViewById(R.id.statistics);
        addrEdit = findViewById(R.id.addrEdit);
        portEdit = findViewById(R.id.portEdit);
        connectButton = findViewById(R.id.connectButton);

        // UI broadcast receiver
        Context ui = this;
        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                String status = intent.getStringExtra(UI_STATUS);
                assert(status != null);
                if (status.equals(UI_BREAK)) {
                    stopVPNService();
                } else {
                    if (status.equals(UI_CREATE)) {
                        connectButton.setClickable(true);
                        connectButton.setText(R.string.disconnect_text);
                        connected = true;
                    } else if (status.equals(UI_FAILED)) {
                        Toast.makeText(ui, "Failed to connect", Toast.LENGTH_SHORT).show();
                        stopVPNService();
                    } else {
                        // Update statistics
                        statisticsView.setText(status);
                    }
                }
            }
        };
        IntentFilter filter = new IntentFilter();
        filter.addAction(UI_FILTER);
        registerReceiver(receiver, filter);
    }

    // Try to connect
    protected void startVPNService() {
        Intent intent = VpnService.prepare(MainActivity.this);
        // Prepare only once
        if (intent != null) {
            startActivityForResult(intent, VPN_INTENT_REQUEST);
        } else {
            onActivityResult(VPN_INTENT_REQUEST, RESULT_OK, null);
        }
    }

    public void click(View view) {
        if (connected) {
            stopVPNService();
        } else {
            startVPNService();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (requestCode == VPN_INTENT_REQUEST) {
            Intent intent = new Intent(this, VPNService.class);
            String addr = addrEdit.getText().toString();
            String port = portEdit.getText().toString();
            if (addr.isEmpty() || port.isEmpty()) {
                Toast.makeText(this, "Address/Port field can not be empty", Toast.LENGTH_SHORT).show();
            } else {
                connectButton.setClickable(false);
                intent.putExtra(INTENT_ADDR, addr);
                intent.putExtra(INTENT_PORT, port);
                Log.i(TAG, "System VPN service begins running");
                startService(intent);
            }
        } else {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    // Terminate service
    protected void stopVPNService() {
        Log.d(TAG, "Trying to stop VPN service");
        Intent stop = new Intent();
        stop.setAction(VPNService.COMMAND);
        sendBroadcast(stop);

        statisticsView.setText(R.string.default_statistics);
        connectButton.setClickable(true);
        connectButton.setText(R.string.connect_text);
        connected = false;
    }

    @Override
    protected void onDestroy() {
        stopVPNService();
        super.onDestroy();
    }
}
