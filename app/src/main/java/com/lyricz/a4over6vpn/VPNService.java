package com.lyricz.a4over6vpn;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.net.VpnService;
import android.widget.Toast;

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

    // Variables
    int sockfd;
    BackendThread backend;

    @SuppressLint("Assert")
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        sockfd = open();
        String info = request();
        if (info.isEmpty()) {
            Toast.makeText(this, "Failed to connect", Toast.LENGTH_SHORT).show();
            stopSelf();
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

            // TODO: return pattern
            return START_STICKY;
        }
        return START_STICKY;
    }

    // Timer
    protected void startTimer() {
        Timer timer = new Timer();
        TimerTask task = new TimerTask() {
            @Override
            public void run() {
                String info = tik();
                sockfd = info.isEmpty() ? -1 : sockfd;

                Intent ui = new Intent();
                ui.setAction(MainActivity.UI_FILTER);
                ui.putExtra(MainActivity.UI_STATUS, info);
                sendBroadcast(ui);

                if (sockfd == -1) {
                    cancel();
                    stopSelf();
                }
            }
        };

        timer.schedule(task, NO_DELAY, TIMER_INTERVAL);
    }

    @Override
    public void onDestroy() {
        terminate();
        super.onDestroy();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    // Open a new socket
    public native int open();

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
