sc stop AmneziaWGTunnel$AresVPN
sc delete AmneziaWGTunnel$AresVPN
taskkill /IM "AresVPNClient-service.exe" /F
taskkill /IM "AresVPNClient.exe" /F
exit /b 0
