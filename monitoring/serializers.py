from rest_framework import serializers
from .models import SensorStation, TelemetryRecord, DisasterAlertLog

class SensorStationSerializer(serializers.ModelSerializer):
    class Meta:
        model = SensorStation
        fields = '__all__'

class TelemetryRecordSerializer(serializers.ModelSerializer):
    station_name = serializers.ReadOnlyField(source='station.name')
    station_id = serializers.ReadOnlyField(source='station.station_id')

    class Meta:
        model = TelemetryRecord
        fields = '__all__'

class DisasterAlertLogSerializer(serializers.ModelSerializer):
    class Meta:
        model = DisasterAlertLog
        fields = '__all__'
