# Test MQTT doorbell/cmd/settings using HiveMQ Cloud REST API

$HIVEMQ_CLUSTER = "1cc4e72660cd4655a75fac2f454c5a76.s1.eu.hivemq.cloud"
$MQTT_USERNAME = "esp_doorbell"
$MQTT_PASSWORD = "Hcmus123"

$base64AuthInfo = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes("${MQTT_USERNAME}:${MQTT_PASSWORD}"))

$topic = "doorbell/cmd/settings"

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

$url = "https://$HIVEMQ_CLUSTER/api/v1/mqtt/publish"

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

    Write-Host "SUCCESS! Message published to MQTT broker" -ForegroundColor Green
    Write-Host ""
    Write-Host "Response:" -ForegroundColor Cyan
    $response | ConvertTo-Json
    Write-Host ""
    Write-Host "Check ESP32 Serial Monitor for the message!" -ForegroundColor Yellow
}
catch {
    Write-Host "FAILED to publish message" -ForegroundColor Red
    Write-Host ""
    Write-Host "Error:" -ForegroundColor Red
    Write-Host $_.Exception.Message
    Write-Host ""
    
    if ($_.Exception.Response) {
        $reader = New-Object System.IO.StreamReader($_.Exception.Response.GetResponseStream())
        $responseBody = $reader.ReadToEnd()
        Write-Host "Response:" -ForegroundColor Red
        Write-Host $responseBody
    }
}
