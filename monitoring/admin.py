from django.contrib import admin
from .models import SensorStation, TelemetryRecord, DisasterAlertLog

@admin.register(SensorStation)
class SensorStationAdmin(admin.ModelAdmin):
    list_display = ('name', 'station_id', 'latitude', 'longitude', 'is_active', 'updated_at')
    search_fields = ('name', 'station_id', 'description')
    list_filter = ('is_active', 'created_at')

@admin.register(TelemetryRecord)
class TelemetryRecordAdmin(admin.ModelAdmin):
    list_display = ('timestamp', 'station', 'pga_g', 'magnitude', 'seismic_status', 'water_level_cm', 'flood_status', 'tds_ppm')
    list_filter = ('seismic_status', 'flood_status', 'station', 'timestamp')
    search_fields = ('station__name', 'station__station_id', 'mmi_scale')
    date_hierarchy = 'timestamp'

@admin.register(DisasterAlertLog)
class DisasterAlertLogAdmin(admin.ModelAdmin):
    list_display = ('timestamp', 'title', 'disaster_type', 'severity', 'magnitude', 'water_level_cm', 'station')
    list_filter = ('disaster_type', 'severity', 'timestamp')
    search_fields = ('title', 'description')
