package com.lyricz.a4over6vpn;

import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.VpnService;
import android.util.Log;

import java.util.Objects;
import java.util.Timer;
import java.util.TimerTask;

public class VPNService extends VpnService {
    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    // Parameters
    static int NO_DELAY = 0;
    static int TIMER_INTERVAL = 1000;
    static int MTU = 1500;

    static String TAG = "VPNService";
    static String COMMAND = "VPNCommand";

    // Variables
    int sockfd;
    BackendThread backend;

    @SuppressLint("Assert")
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Receive MainActivity command
        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                // The only way to stop
                Log.d(TAG, "Get stop request from main");
                terminate();
                unregisterReceiver(this);
                stopSelf();
            }
        };
        IntentFilter filter = new IntentFilter();
        filter.addAction(COMMAND);
        registerReceiver(receiver, filter);

        // Build connection
        String addr = intent.getStringExtra(MainActivity.INTENT_ADDR);
        String port = intent.getStringExtra(MainActivity.INTENT_PORT);
        Log.d(TAG, "Start VPN Service with " + addr + "@" + port);

        sockfd = open(addr, port);
        String info = request();

        if (info.isEmpty()) {
            notifyUI(MainActivity.UI_FAILED);
        } else {
            // The socket for VPN itself must be protected
            protect(sockfd);

            String[] settings = info.split(" ");
            assert (settings.length == 5);

            // Configure
            Builder builder = new Builder();
            int tunfd = Objects.requireNonNull(builder.setSession("4over6 VPN Session")
                    .addAddress(settings[0], 24)    // ip
                    .addRoute(settings[1], 0)       // route
                    .addDnsServer(settings[2])                  // dns0
                    .addDnsServer(settings[3])                  // dns1
                    .addDnsServer(settings[4])                  // dns2
                    .setMtu(MTU)                                // mtu
                    .establish()).getFd();

            backend = new BackendThread(tunfd);
            backend.start();

            startTimer();

            notifyUI(MainActivity.UI_CREATE);
            return START_NOT_STICKY;
        }
        return START_NOT_STICKY;
    }

    protected void notifyUI(String info) {
        Intent ui = new Intent();
        ui.setAction(MainActivity.UI_FILTER);
        ui.putExtra(MainActivity.UI_STATUS, info);
        sendBroadcast(ui);
    }

    // Timer
    protected void startTimer() {
        Timer timer = new Timer();
        TimerTask task = new TimerTask() {
            @Override
            public void run() {
                String info = tik();
                if (info.isEmpty()) {
                    sockfd = -1;
                    info = MainActivity.UI_BREAK;
                    cancel();
                }
                notifyUI(info);
            }
        };

        timer.schedule(task, NO_DELAY, TIMER_INTERVAL);
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "VPN Service destroyed");
        super.onDestroy();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    // Open a new socket
    public native int open(String addr, String port);

    // Request for a VPN address
    public native String request();

    // Tik-tok
    public native String tik();

    // Run backend
    public native void backend(int fd);

    // Terminate all
    public native void terminate();

    // Thread supporting backend
    class BackendThread extends Thread {
        int tunfd;

        public BackendThread(int tunfd) {
            this.tunfd = tunfd;
        }

        @Override
        public void run() {
            backend(tunfd);
        }
    }
}
