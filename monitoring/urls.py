from django.urls import path
from . import views

app_name = 'monitoring'

urlpatterns = [
    path('', views.dashboard_view, name='dashboard'),
    path('api/telemetry/', views.telemetry_api, name='telemetry_api'),
    path('api/telemetry/latest/', views.latest_telemetry_api, name='latest_telemetry_api'),
    path('api/telemetry/export/', views.export_csv_api, name='export_csv_api'),
    path('api/bmkg/feed/', views.bmkg_proxy_api, name='bmkg_proxy_api'),
]
