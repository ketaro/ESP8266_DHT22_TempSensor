usehost = "";

function handleNavClick(e) {
    var selectedTab = $(this).data('tab');
    
    $('.tabcontent').css('display', 'none');
    $('#' + selectedTab).css('display', 'block');
    $('nav a').removeClass('active');
    $(this).addClass('active');
}

function displayAlert(msg) {
    console.log('[alert] ' + msg);
    $('#alert p').html(msg);
    $('#alert').show();
}

function updateNetworkConfig(data) {
    if (data.hasOwnProperty('ssid'))
        $('input[name=ssid]').val( data['ssid'] );

    if (data.hasOwnProperty('pw'))
        $('input[name=wifi_pw]').attr('placeholder', "Saved.  Update to change.");
}


function updateSettingsConfig(data) {
    if (data.hasOwnProperty('db_name'))
        $('input[name=db_name]').val( data['db_name'] );

    if (data.hasOwnProperty('db_host'))
        $('input[name=db_host]').val( data['db_host'] );

    if (data.hasOwnProperty('db_port'))
        $('input[name=db_port]').val( data['db_port'] );

    if (data.hasOwnProperty('db_measurement'))
        $('input[name=db_measurement]').val( data['db_measurement'] );

    if (data.hasOwnProperty('location')) {
        $('input[name=location]').val( data['location'] );
        $('.location').html( data['location'] );
    }

    if (data.hasOwnProperty('interval'))
        $('select[name=interval]').val( data['interval'] );
}


// Read System Config
function getConfigData() {
    $.ajax({
        url: usehost + "/config",
        dataType: 'json',
    }).done(function( data ) {
        console.log(data);
        
        if (data.hasOwnProperty('hostname')) {
            $('.conf_hostname').html( data['hostname'] );
            $('input[name=hostname]').val( data['hostname'] );
        }

        if (data.hasOwnProperty('mac')) {
            $('.conf_mac').html( data['mac'] );
            $('input[name=mac]').val( data['mac'] );
        }

        if (data.hasOwnProperty('ino_ver'))
            $('.conf_ino_ver').html( data['ino_ver'] );

        if (data.hasOwnProperty('net'))
            updateNetworkConfig( data['net'] );

        if (data.hasOwnProperty('db'))
            updateSettingsConfig( data['db'] );
        
       
    }).fail(function( data ) {
        displayAlert('Error retrieving config data');
    });
    
}

// Read Sensor Values
function getSensorReading() {
    $.ajax({
        url: usehost + "/sensors",
        dataType: 'json',
    }).done(function( data ) {
        console.log(data);

        if (data.hasOwnProperty('temp'))
            $('.reading-temp').html( data['temp'] + "&deg;F" );
        
        if (data.hasOwnProperty('hum'))
            $('.reading-humidity').html( data['hum'] + "%" );
        
        if (data.hasOwnProperty('hidx'))
            $('.reading-hidx').html( data['hidx'] );
        
        setTimeout(getSensorReading, 10000);

    }).fail(function( data ) {
        displayAlert('Error retrieving sensor readings');
    });
    
}


function saveSettings() {
    $('#btn_settingsSave').prop("disabled", true);
    $('#btn_settingsSave').data('origtext', $('#btn_settingsSave').html());
    $('#btn_settingsSave').html("Saving...");
    
    var data = {
        db_type: $('select[name=dbtype]').val(),
        db_host: $('input[name=db_host]').val(),
        db_port: $('input[name=db_port]').val(),
        db_name: $('input[name=db_name]').val(),
        db_measurement: $('input[name=db_measurement]').val(),
        location: $('input[name=location]').val(),
        interval: $('select[name=interval]').val(),
    }
    
    $.ajax({
        url: usehost + "/settings",
        type: "POST",
        data: data
    }).done(function (data) {
        $('#btn_settingsSave').prop("disabled", false);
        $('#btn_settingsSave').html($('#btn_settingsSave').data('origtext'));

        // Reload Data
        getConfigData();
    }).fail(function( data ) {
        $('#btn_settingsSave').prop("disabled", false);
        $('#btn_settingsSave').html($('#btn_settingsSave').data('origtext'));
        displayAlert('Error Saving Settings');
    });
}


function saveSecurity() {
    $('#btn_accessSave').prop("disabled", true);
    $('#btn_accessSave').data('origtext', $('#btn_accessSave').html());
    $('#btn_accessSave').html("Saving...");
    
    var data = {
        http_user: $('input[name=http_user]').val(),
        http_pw: $('input[name=http_pw]').val(),
    }
    
    $.ajax({
        url: usehost + "/settings",
        type: "POST",
        data: data
    }).done(function (data) {
        $('#btn_accessSave').prop("disabled", false);
        $('#btn_accessSave').html($('#btn_accessSave').data('origtext'));

        // Reload Data
        getConfigData();
    }).fail(function( data ) {
        $('#btn_accessSave').prop("disabled", false);
        $('#btn_accessSave').html($('#btn_accessSave').data('origtext'));
        displayAlert('Error Saving Settings');
    });
}


function saveNetwork() {
    $('#btn_networkSave').prop("disabled", true);
    $('#btn_networkSave').data('origtext', $('#btn_networkSave').html());
    $('#btn_networkSave').html("Saving...");

    var data = {
        ssid: $('input[name=ssid]').val(),
        wifi_pw: $('input[name=wifi_pw]').val(),
        network_type: $('select[name=network_type]').val(),
        hostname: $('input[name=hostname]').val(),
    }

    $.ajax({
        url: usehost + "/network",
        type: "POST",
        data: data
    }).done(function (data) {
        $('#btn_networkSave').prop("disabled", false);
        $('#btn_networkSave').html($('#btn_networkSave').data('origtext'));

        // Reload Data
        getConfigData();
    }).fail(function( data ) {
        $('#btn_networkSave').prop("disabled", false);
        $('#btn_networkSave').html($('#btn_networkSave').data('origtext'));
        displayAlert('Error Saving Network Settings!');
    });    
}

function factoryReset() {
    $('#btn_factoryReset').prop("disabled", true);
    $('#btn_factoryReset').data('origtext', $('#btn_factoryReset').html());
    $('#btn_factoryReset').html("Requesting Reset...");

    // TODO: Maybe add a confirmation?
    $.ajax({
        url: usehost + "/reset",
        type: "POST",
        data: { reset: 1 }
    }).done(function (data) {
        $('#btn_factoryReset').prop("disabled", false);
        $('#btn_factoryReset').html($('#btn_factoryReset').data('origtext'));

        // Reload Data
        getConfigData();
    }).fail(function( data ) {
        $('#btn_factoryReset').prop("disabled", false);
        $('#btn_factoryReset').html($('#btn_factoryReset').data('origtext'));
        displayAlert('Error Requesting Factory Reset');
    });    

}


(function() {
    $('nav a').click(handleNavClick);
    $('nav a.default').click();

    // button listeners
    $('#btn_settingsSave').click(saveSettings);
    $('#btn_networkSave').click(saveNetwork);
    $('#btn_accessSave').click(saveSecurity);
    $('#btn_factoryReset').click(factoryReset);
    $('#alert .close').click(function() { $('#alert').hide(); });

    getConfigData();
    getSensorReading();
})();
