from django.db import models
from django.utils import timezone

class SensorStation(models.Model):
    """Model untuk Posko / Stasiun Mikrokontroler ESP32"""
    station_id = models.CharField(max_length=64, unique=True, verbose_name="ID Stasiun")
    name = models.CharField(max_length=255, verbose_name="Nama Posko / Lokasi")
    latitude = models.FloatField(default=-6.2088, verbose_name="Latitude (Lintang)")
    longitude = models.FloatField(default=106.8456, verbose_name="Longitude (Bujur)")
    description = models.TextField(blank=True, null=True, verbose_name="Deskripsi / Catatan")
    is_active = models.BooleanField(default=True, verbose_name="Status Aktif")
    created_at = models.DateTimeField(auto_now_add=True, verbose_name="Waktu Registrasi")
    updated_at = models.DateTimeField(auto_now=True, verbose_name="Update Terakhir")

    class Meta:
        verbose_name = "Stasiun Sensor"
        verbose_name_plural = "Daftar Stasiun Sensor"
        ordering = ['-updated_at']

    def __str__(self):
        return f"{self.name} ({self.station_id})"


class TelemetryRecord(models.Model):
    """Model Penyimpanan Data Telemetri Sensor Realtime (Time-Series)"""
    STATUS_CHOICES = [
        ('NORMAL', 'Normal'),
        ('WASPADA', 'Waspada'),
        ('SIAGA', 'Siaga'),
        ('AWAS', 'Awas / Bahaya'),
    ]

    station = models.ForeignKey(SensorStation, on_delete=models.CASCADE, related_name='telemetries', null=True, blank=True)
    
    # 1. Parameter Seismik (Gempa & Getaran)
    pga_g = models.FloatField(default=0.002, verbose_name="PGA (g)")
    gal = models.FloatField(default=1.96, verbose_name="Akselerasi Gal (cm/s²)")
    magnitude = models.FloatField(default=0.0, verbose_name="Estimasi Magnitudo (SR)")
    frequency_hz = models.FloatField(default=0.2, verbose_name="Frekuensi (Hz)")
    mmi_scale = models.CharField(max_length=64, default="I (Tidak Terasa)", verbose_name="Skala MMI")
    seismic_status = models.CharField(max_length=32, choices=STATUS_CHOICES, default='NORMAL', verbose_name="Status Gempa")

    # 2. Parameter Muka Air (Banjir)
    water_level_cm = models.FloatField(default=42.5, verbose_name="Ketinggian Air (cm)")
    flood_status = models.CharField(max_length=32, choices=STATUS_CHOICES, default='NORMAL', verbose_name="Status Banjir")

    # 3. Parameter Hujan
    rain_rate_mmh = models.FloatField(default=0.0, verbose_name="Curah Hujan (mm/jam)")
    rain_raw_analog = models.IntegerField(default=4095, verbose_name="Nilai Analog Hujan")
    rain_condition = models.CharField(max_length=64, default="Cerah / Tidak Hujan", verbose_name="Kondisi Hujan")

    # 4. Parameter Kualitas Air (TDS)
    tds_ppm = models.FloatField(default=148.0, verbose_name="Partikel TDS (PPM)")
    water_quality = models.CharField(max_length=64, default="Air Bersih / Layak", verbose_name="Kualitas Air")

    # 5. Timestamp & Meta
    timestamp = models.DateTimeField(default=timezone.now, db_index=True, verbose_name="Waktu Perekaman")

    class Meta:
        verbose_name = "Log Telemetri Sensor"
        verbose_name_plural = "Riwayat Telemetri Sensor"
        ordering = ['-timestamp']

    def __str__(self):
        return f"[{self.timestamp.strftime('%Y-%m-%d %H:%M:%S')}] PGA: {self.pga_g}g | Air: {self.water_level_cm}cm | TDS: {self.tds_ppm}ppm"


class DisasterAlertLog(models.Model):
    """Model Pencatatan Aktivasi Early Warning System (EWS)"""
    DISASTER_TYPES = [
        ('QUAKE', 'Gempa Bumi / Seismik'),
        ('FLOOD', 'Banjir / Luapan Air'),
        ('COMBINED', 'Bencana Multi-Kondisi'),
    ]

    station = models.ForeignKey(SensorStation, on_delete=models.SET_NULL, null=True, blank=True)
    disaster_type = models.CharField(max_length=32, choices=DISASTER_TYPES, default='QUAKE', verbose_name="Jenis Bencana")
    title = models.CharField(max_length=255, verbose_name="Judul Peringatan")
    description = models.TextField(verbose_name="Keterangan Peringatan")
    severity = models.CharField(max_length=32, default="AWAS", verbose_name="Tingkat Keparahan")
    magnitude = models.FloatField(null=True, blank=True, verbose_name="Magnitudo")
    water_level_cm = models.FloatField(null=True, blank=True, verbose_name="Tinggi Air (cm)")
    timestamp = models.DateTimeField(default=timezone.now, verbose_name="Waktu Kejadian")

    class Meta:
        verbose_name = "Peringatan Dini (EWS Alert)"
        verbose_name_plural = "Riwayat Peringatan Dini EWS"
        ordering = ['-timestamp']

    def __str__(self):
        return f"{self.title} ({self.severity}) - {self.timestamp.strftime('%H:%M:%S')}"
