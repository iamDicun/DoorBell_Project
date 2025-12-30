# Test MQTT doorbell/cmd/settings using HiveMQ Cloud REST API
# This script publishes a settings sync message to test if ESP32 receives it

# HiveMQ Cloud credentials
$HIVEMQ_CLUSTER = "1cc4e72660cd4655a75fac2f454c5a76.s1.eu.hivemq.cloud"
$MQTT_USERNAME = "esp_doorbell"
$MQTT_PASSWORD = "Hcmus123"

# Create Basic Auth header
$base64AuthInfo = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("${MQTT_USERNAME}:${MQTT_PASSWORD}"))

# MQTT topic and payload
$topic = "doorbell/cmd/settings"

# Test payload - sync_settings with all device settings
$payload = @{
    action = "sync_settings"
    speaker_volume = 75
    pir_enabled = $true
    alarm_enabled = $false
    notifications_enabled = $true
    do_not_disturb = $false
    alarm_auto_play = $true
    temp_enabled = $true
} | ConvertTo-Json -Compress

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Testing MQTT: doorbell/cmd/settings" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "MQTT Broker: $HIVEMQ_CLUSTER" -ForegroundColor Yellow
Write-Host "Topic: $topic" -ForegroundColor Yellow
Write-Host "Payload: $payload" -ForegroundColor Yellow
Write-Host ""

# HiveMQ Cloud REST API endpoint for publishing
$url = "https://$HIVEMQ_CLUSTER/api/v1/mqtt/publish"

# Request body for HiveMQ REST API
$body = @{
    topic = $topic
    payload = $payload
    qos = 1
    retain = $false
} | ConvertTo-Json

Write-Host "Publishing to HiveMQ Cloud REST API..." -ForegroundColor Green
Write-Host ""

try {
    $response = Invoke-RestMethod -Uri $url -Method Post `
        -Headers @{
            "Authorization" = "Basic $base64AuthInfo"
            "Content-Type" = "application/json"
        } `
        -Body $body

    Write-Host "✓ SUCCESS! Message published to MQTT broker" -ForegroundColor Green
    Write-Host ""
    Write-Host "Response:" -ForegroundColor Cyan
    $response | ConvertTo-Json
    Write-Host ""
    Write-Host "Check ESP32 Serial Monitor to see if it received the message!" -ForegroundColor Yellow
    Write-Host ""
}
catch {
    Write-Host "✗ FAILED to publish message" -ForegroundColor Red
    Write-Host ""
    Write-Host "Error Details:" -ForegroundColor Red
    Write-Host $_.Exception.Message
    Write-Host ""
    
    if ($_.Exception.Response) {
        $reader = New-Object System.IO.StreamReader($_.Exception.Response.GetResponseStream())
        $responseBody = $reader.ReadToEnd()
        Write-Host "Response Body:" -ForegroundColor Red
        Write-Host $responseBody
    }
    
    Write-Host ""
    Write-Host "Common issues:" -ForegroundColor Yellow
    Write-Host "  1. Check if HiveMQ Cloud cluster is active" -ForegroundColor Yellow
    Write-Host "  2. Verify username/password are correct" -ForegroundColor Yellow
    Write-Host "  3. Check if REST API is enabled on HiveMQ Cloud" -ForegroundColor Yellow
    Write-Host "  4. Verify cluster URL is correct" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Expected ESP32 Serial Output:" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "MQTT CALLBACK TRIGGERED" -ForegroundColor Gray
Write-Host "Topic: doorbell/cmd/settings" -ForegroundColor Gray
Write-Host "Message with action sync_settings" -ForegroundColor Gray
Write-Host "Settings syncing from server..." -ForegroundColor Gray
Write-Host "Speaker volume applied, PIR enabled..." -ForegroundColor Gray
