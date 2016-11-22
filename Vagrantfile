VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|

  config.vm.hostname = "vagrant"
  config.vm.box = "ubuntu/precise64"

  config.vm.network :private_network, ip: "10.10.10.10"

  config.vm.provider :virtualbox do |virtualbox|
    # Modify the vm's allocated ram
    virtualbox.memory = 512
	config.vm.synced_folder ".", "/vagrant"
  end
  config.vm.provision "shell", path: "bootstrap.sh"

end
