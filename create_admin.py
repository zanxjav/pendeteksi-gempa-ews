import os
import django

os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'geoshield.settings')
django.setup()

from django.contrib.auth.models import User
from monitoring.models import SensorStation

# Create Superuser if not exists
if not User.objects.filter(username='admin').exists():
    User.objects.create_superuser('admin', 'admin@example.com', 'admin123')
    print("[SUCCESS] Superuser 'admin' berhasil dibuat (Password: admin123)")
else:
    print("[INFO] Superuser 'admin' sudah ada.")

# Create Default Station
station, created = SensorStation.objects.get_or_create(
    station_id="ST-01-ESP32",
    defaults={
        "name": "Stasiun Posko Bencana 01",
        "latitude": -6.2088,
        "longitude": 106.8456,
        "description": "Stasiun Utama Monitoring Gempa & Banjir Terpadu"
    }
)
if created:
    print("[SUCCESS] Stasiun default berhasil didaftarkan ke database.")
else:
    print("[INFO] Stasiun default sudah aktif di database.")
