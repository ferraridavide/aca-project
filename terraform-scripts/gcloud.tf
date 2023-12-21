# Questo codice è compatibile con Terraform 4.25.0 e versioni precedenti compatibili con 4.25.0.
# Per informazioni sulla convalida di questo codice Terraform, visita la pagina https://developer.hashicorp.com/terraform/tutorials/gcp-get-started/google-cloud-platform-build#format-and-validate-the-configuration

provider "google" {
  credentials = file("aca-first-project-d931c651499a.json")
  project = "aca-first-project"
  region = "us-central1"
}

variable "instaces-count" {
  default     = 4
  description = "Number of instances"
}

resource "google_compute_instance" "example" {
  boot_disk {
    auto_delete = true
    device_name = "example-disk"

    initialize_params {
      image = "projects/debian-cloud/global/images/debian-11-bullseye-v20231212"
      size  = 50
      type  = "pd-standard"
    }

    mode = "READ_WRITE"
  }

  can_ip_forward      = false
  deletion_protection = false
  enable_display      = false

  labels = {
    goog-ec-src = "vm_add-tf"
  }

  machine_type = "e2-custom-2-7168"

  metadata = {
    ssh-keys = "[REDACTED]"
  }

  count = var.instaces-count
  name = "node-${count.index}"

  network_interface {
    access_config {
      network_tier = "PREMIUM"
    }

    subnetwork = "projects/aca-first-project/regions/us-central1/subnetworks/default"
  }

  scheduling {
    automatic_restart   = true
    on_host_maintenance = "MIGRATE"
    preemptible         = false
    provisioning_model  = "STANDARD"
  }

  service_account {
    email  = "600977192316-compute@developer.gserviceaccount.com"
    scopes = ["https://www.googleapis.com/auth/devstorage.read_only", "https://www.googleapis.com/auth/logging.write", "https://www.googleapis.com/auth/monitoring.write", "https://www.googleapis.com/auth/service.management.readonly", "https://www.googleapis.com/auth/servicecontrol", "https://www.googleapis.com/auth/trace.append"]
  }

  shielded_instance_config {
    enable_integrity_monitoring = true
    enable_secure_boot          = false
    enable_vtpm                 = true
  }

  zone = "us-central1-a"
}