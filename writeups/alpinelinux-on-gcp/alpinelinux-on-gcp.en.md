# Alpine Linux on Google Cloud Platform
Short notes on launching Alpine Linux on GCP. The GCP support of Alpine Linux
still remains beta so use this guide is also as an experimental note.

The bios+tiny-cloud images are useable on the GCP. The EFI images are unusable
because GCP only offers EFI with secure boot. The cloud-init images are unusable
because it's unable to install SSH public keys. So you're left with two options:

- x86_64 • bios • tiny • vm
- x86_64 • bios • tiny • metal

The current problem with tiny-init is that it installs all the SSH public keys
in the `alpine` user directory, not in the user directory specified in the name
of the key. As this is not the GCP's expectation, gcloud ssh command or the
browser ssh client won't work. But you can use any of the key to login as
`alpine`.

## Importing the images
As GCP doesn't do public OS images, to use Alpine Linux on GCP, you have
import the raw images from the official website yourself.

First, download the images and convert them to qcow2 images.

```sh
wget \
    https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/cloud/gcp_alpine-3.20.2-x86_64-bios-tiny-r0.raw \
    https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/cloud/gcp_alpine-3.20.2-x86_64-bios-tiny-metal-r0.raw

for f in *.raw
do
    qemu-img convert -f raw -O qcow2 "$f" $(basename -s .raw "$f").qcow2
done
```

Upload the qcow2 files to a cloud storage bucket. If the bucket has fine-grained
permissions policy, you have to edit the permissions of the uploaded files so
that the image service can access them. It seems that the only way to find out
the user of the image creation service user is by inspecting the error message.
You should be fine if the bucket is publicly accessible or has uniform policy
because the service will create a permission for you.

Go to **Compute Engine** - **Migrate to Virtual Machines** - **IMAGE IMPORTS** -
**CREATE IMAGE**. Name the new image, set the source to the image in the bucket,
select the region. Make sure **Skip OS adaptation** is on(the process will fail
because Alpine Linux is not supported by GCP).

## Planting SSH keys
Tiny-init won't plant the public SSH keys on boot. You have to push the event by
trying to SSH or serial console. Then tiny-init will install the SSH keys.
