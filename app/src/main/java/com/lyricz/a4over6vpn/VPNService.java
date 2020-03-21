package com.lyricz.a4over6vpn;

import android.app.Service;
import android.content.Intent;
import android.net.VpnService;
import android.os.IBinder;

import java.util.Objects;

public class VPNService extends VpnService {
    static int MTU = 1500;

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // The socket for VPN itself must be protected
        int sockfd = intent.getIntExtra("socket", -1);
        protect(sockfd);

        // Configure
        Builder builder = new Builder();
        int tunnel_fd = Objects.requireNonNull(builder.setSession("4over6 VPN Session")
                .addAddress(Objects.requireNonNull(intent.getStringExtra("ip_addr")), 24)
                .addRoute(Objects.requireNonNull(intent.getStringExtra("route")), 0)
                .addDnsServer(Objects.requireNonNull(intent.getStringExtra("dns0")))
                .addDnsServer(Objects.requireNonNull(intent.getStringExtra("dns1")))
                .addDnsServer(Objects.requireNonNull(intent.getStringExtra("dns2")))
                .setMtu(MTU)
                .establish()).getFd();

        // 'START_STICKY' means when service being killed, next time we'll rebuild one
        return START_STICKY;
    }

    @Override
    public void onDestroy() {

        super.onDestroy();
    }
}
