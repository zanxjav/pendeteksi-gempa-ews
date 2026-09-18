import csv
from django.shortcuts import render
from django.http import HttpResponse, JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.utils import timezone
from rest_framework.decorators import api_view
from rest_framework.response import Response
from rest_framework import status
import requests

from .models import SensorStation, TelemetryRecord, DisasterAlertLog
from .serializers import TelemetryRecordSerializer, SensorStationSerializer

def dashboard_view(request):
    """Render Web Dashboard Utama GeoShield EWS"""
    # Ambil atau buat stasiun default
    station, created = SensorStation.objects.get_or_create(
        station_id="ST-01-ESP32",
        defaults={
            "name": "Pendeteksi Gempa EWS ITERA - ESP32 C3",
            "latitude": -5.35824,
            "longitude": 105.31465,
            "description": "Sistem Peringatan Dini Gempa Bumi & Multi-Bencana ITERA Lampung"
        }
    )

    latest_telemetry = TelemetryRecord.objects.filter(station=station).first()
    recent_logs = TelemetryRecord.objects.all()[:20]

    context = {
        'station': station,
        'latest_telemetry': latest_telemetry,
        'recent_logs': recent_logs,
    }
    return render(request, 'monitoring/dashboard.html', context)


@csrf_exempt
@api_view(['GET', 'POST'])
def telemetry_api(request):
    """
    Endpoint REST API untuk menerima telemetri dari mikrokontroler ESP32
    Method POST: Terima payload JSON dari ESP32
    Method GET: Ambil 30 rekaman data telemetri terbaru
    """
    if request.method == 'POST':
        data = request.data
        if not data:
            return Response({"error": "Payload JSON kosong"}, status=status.HTTP_400_BAD_REQUEST)

        # 1. Ekstrak Identitas Stasiun
        station_id = data.get('station_id', 'ST-01-ESP32')
        station_name = data.get('station_name')
        loc = data.get('location', {})
        lat = loc.get('lat') or data.get('lat')
        lng = loc.get('lng') or data.get('lng')

        station, _ = SensorStation.objects.get_or_create(
            station_id=station_id,
            defaults={
                'name': station_name or f"Stasiun {station_id}",
                'latitude': float(lat) if lat else -6.2088,
                'longitude': float(lng) if lng else 106.8456,
            }
        )

        # Auto-update koordinat stasiun jika ESP32 mengirim lokasi baru
        if lat and lng:
            station.latitude = float(lat)
            station.longitude = float(lng)
            if station_name:
                station.name = station_name
            station.save()

        # 2. Ekstrak Nilai Sensor
        seismic = data.get('seismic', {})
        pga_g = float(seismic.get('pga', data.get('pga', 0.002)))
        gal = pga_g * 980.665

        # Hitung estimasi Magnitudo & MMI
        if gal < 1.4:
            mmi = "I (Tidak Terasa)"
            mag = 0.0
            seismic_status = "NORMAL"
        elif gal < 9.0:
            mmi = "II - III (Getaran Ringan)"
            mag = 3.0 + (gal / 9.0)
            seismic_status = "WASPADA"
        elif gal < 30.0:
            mmi = "IV - V (Sedang / Nyata)"
            mag = 4.2 + (gal / 40.0)
            seismic_status = "SIAGA"
        else:
            mmi = "VI+ (Kuat / Merusak)"
            mag = 5.5 + min(2.5, gal / 150.0)
            seismic_status = "AWAS"

        flood = data.get('flood', {})
        water_level = float(flood.get('waterLevelCm', data.get('water_level', 42.5)))
        if water_level >= 150:
            flood_status = "AWAS"
        elif water_level >= 100:
            flood_status = "SIAGA"
        elif water_level >= 75:
            flood_status = "WASPADA"
        else:
            flood_status = "NORMAL"

        rain = data.get('rain', {})
        rain_raw = int(rain.get('rawAnalog', data.get('rain_raw', 4095)))
        rain_rate = float(rain.get('rateMmh', data.get('rain_rate', max(0, ((4095 - rain_raw) / 4095) * 80))))
        
        if rain_rate < 0.5:
            rain_cond = "Cerah / Kering"
        elif rain_rate < 20:
            rain_cond = "Hujan Sedang"
        else:
            rain_cond = "Hujan Lebat / Ekstrem"

        tds = data.get('tds', {})
        tds_ppm = float(tds.get('ppm', data.get('tds_ppm', 148.0)))
        water_qual = "Air Bersih / Layak" if tds_ppm <= 300 else ("Tercemar Sedang" if tds_ppm <= 600 else "Keruh / Lumpur")

        # 3. Simpan ke Database
        record = TelemetryRecord.objects.create(
            station=station,
            pga_g=pga_g,
            gal=gal,
            magnitude=mag,
            frequency_hz=float(seismic.get('freq', 0.2)),
            mmi_scale=mmi,
            seismic_status=seismic_status,
            water_level_cm=water_level,
            flood_status=flood_status,
            rain_rate_mmh=rain_rate,
            rain_raw_analog=rain_raw,
            rain_condition=rain_cond,
            tds_ppm=tds_ppm,
            water_quality=water_qual,
            timestamp=timezone.now()
        )

        return Response({
            "status": "success",
            "message": "Data telemetri berhasil disimpan ke database Django",
            "record_id": record.id,
            "station": station.name
        }, status=status.HTTP_201_CREATED)

    # GET Request: Ambil data telemetri
    records = TelemetryRecord.objects.all()[:30]
    serializer = TelemetryRecordSerializer(records, many=True)
    return Response(serializer.data)


@api_view(['GET'])
def latest_telemetry_api(request):
    """Mengambil 1 data telemetri sensor paling baru untuk pembaruan live client"""
    latest = TelemetryRecord.objects.first()
    if not latest:
        return Response({"status": "no_data"}, status=status.HTTP_200_OK)

    serializer = TelemetryRecordSerializer(latest)
    return Response(serializer.data)


@api_view(['GET'])
def export_csv_api(request):
    """Export seluruh log riwayat sensor dari database ke file CSV"""
    response = HttpResponse(content_type='text/csv')
    response['Content-Disposition'] = f'attachment; filename="GeoShield_Telemetry_{int(timezone.now().timestamp())}.csv"'

    writer = csv.writer(response)
    writer.writerow([
        'Waktu', 'ID Stasiun', 'Nama Posko', 'PGA (g)', 'Gal (cm/s2)', 
        'Magnitudo (SR)', 'Skala MMI', 'Status Gempa', 'Tinggi Air (cm)', 
        'Status Banjir', 'Curah Hujan (mm/h)', 'TDS (PPM)', 'Kualitas Air'
    ])

    records = TelemetryRecord.objects.all().order_by('-timestamp')[:500]
    for r in records:
        writer.writerow([
            r.timestamp.strftime('%Y-%m-%d %H:%M:%S'),
            r.station.station_id if r.station else '-',
            r.station.name if r.station else '-',
            r.pga_g,
            r.gal,
            round(r.magnitude, 2),
            r.mmi_scale,
            r.seismic_status,
            r.water_level_cm,
            r.flood_status,
            r.rain_rate_mmh,
            r.tds_ppm,
            r.water_quality
        ])

    return response


@api_view(['GET'])
def bmkg_proxy_api(request):
    """Proxy caching untuk data gempa BMKG & USGS untuk menghindari CORS issue"""
    try:
        res = requests.get('https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_day.geojson', timeout=5)
        return JsonResponse(res.json(), safe=False)
    except Exception as e:
        return JsonResponse({"error": str(e), "status": "offline_fallback"}, status=200)
