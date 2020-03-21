package com.lyricz.a4over6vpn;

import android.annotation.SuppressLint;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.net.VpnService;
import android.os.IBinder;
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

    @SuppressLint("Assert")
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        sockfd = openSocket();
        String info = requestAddress();
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

            setTunnel(tunfd);
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
        closeSocket();
        super.onDestroy();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native int openSocket();

    public native String requestAddress();

    public native String tik();

    public native void setTunnel(int fd);

    public native void closeSocket();
}
