// Copyright 2014 The Gogs Authors. All rights reserved.
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file.

package user

import (
	"io/ioutil"
	"strings"

	"github.com/Unknwon/com"

	"github.com/gogits/gogs/models"
	"github.com/gogits/gogs/modules/auth"
	"github.com/gogits/gogs/modules/base"
	"github.com/gogits/gogs/modules/log"
	"github.com/gogits/gogs/modules/mailer"
	"github.com/gogits/gogs/modules/middleware"
	"github.com/gogits/gogs/modules/setting"
)

const (
	SETTINGS_PROFILE      base.TplName = "user/settings/profile"
	SETTINGS_PASSWORD     base.TplName = "user/settings/password"
	SETTINGS_EMAILS       base.TplName = "user/settings/email"
	SETTINGS_SSH_KEYS     base.TplName = "user/settings/sshkeys"
	SETTINGS_SOCIAL       base.TplName = "user/settings/social"
	SETTINGS_APPLICATIONS base.TplName = "user/settings/applications"
	SETTINGS_DELETE       base.TplName = "user/settings/delete"
	NOTIFICATION          base.TplName = "user/notification"
	SECURITY              base.TplName = "user/security"
)

func Settings(ctx *middleware.Context) {
	ctx.Data["Title"] = ctx.Tr("settings")
	ctx.Data["PageIsSettingsProfile"] = true
	ctx.HTML(200, SETTINGS_PROFILE)
}

func SettingsPost(ctx *middleware.Context, form auth.UpdateProfileForm) {
	ctx.Data["Title"] = ctx.Tr("settings")
	ctx.Data["PageIsSettingsProfile"] = true

	if ctx.HasError() {
		ctx.HTML(200, SETTINGS_PROFILE)
		return
	}

	// Check if user name has been changed.
	if ctx.User.Name != form.UserName {
		if err := models.ChangeUserName(ctx.User, form.UserName); err != nil {
			switch {
			case models.IsErrUserAlreadyExist(err):
				ctx.Flash.Error(ctx.Tr("form.username_been_taken"))
				ctx.Redirect(setting.AppSubUrl + "/user/settings")
			case models.IsErrEmailAlreadyUsed(err):
				ctx.Flash.Error(ctx.Tr("form.email_been_used"))
				ctx.Redirect(setting.AppSubUrl + "/user/settings")
			case models.IsErrNameReserved(err):
				ctx.Flash.Error(ctx.Tr("user.form.name_reserved"))
				ctx.Redirect(setting.AppSubUrl + "/user/settings")
			case models.IsErrNamePatternNotAllowed(err):
				ctx.Flash.Error(ctx.Tr("user.form.name_pattern_not_allowed"))
				ctx.Redirect(setting.AppSubUrl + "/user/settings")
			default:
				ctx.Handle(500, "ChangeUserName", err)
			}
			return
		}
		log.Trace("User name changed: %s -> %s", ctx.User.Name, form.UserName)
		ctx.User.Name = form.UserName
	}

	ctx.User.FullName = form.FullName
	ctx.User.Email = form.Email
	ctx.User.Website = form.Website
	ctx.User.Location = form.Location
	ctx.User.Avatar = base.EncodeMd5(form.Avatar)
	ctx.User.AvatarEmail = form.Avatar
	if err := models.UpdateUser(ctx.User); err != nil {
		ctx.Handle(500, "UpdateUser", err)
		return
	}
	log.Trace("User setting updated: %s", ctx.User.Name)
	ctx.Flash.Success(ctx.Tr("settings.update_profile_success"))
	ctx.Redirect(setting.AppSubUrl + "/user/settings")
}

// FIXME: limit size.
func SettingsAvatar(ctx *middleware.Context, form auth.UploadAvatarForm) {
	defer ctx.Redirect(setting.AppSubUrl + "/user/settings")

	ctx.User.UseCustomAvatar = form.Enable

	if form.Avatar != nil {
		fr, err := form.Avatar.Open()
		if err != nil {
			ctx.Flash.Error(err.Error())
			return
		}

		data, err := ioutil.ReadAll(fr)
		if err != nil {
			ctx.Flash.Error(err.Error())
			return
		}
		if _, ok := base.IsImageFile(data); !ok {
			ctx.Flash.Error(ctx.Tr("settings.uploaded_avatar_not_a_image"))
			return
		}
		if err = ctx.User.UploadAvatar(data); err != nil {
			ctx.Flash.Error(err.Error())
			return
		}
	} else {
		// In case no avatar at all.
		if form.Enable && !com.IsFile(ctx.User.CustomAvatarPath()) {
			ctx.Flash.Error(ctx.Tr("settings.no_custom_avatar_available"))
			return
		}
	}

	if err := models.UpdateUser(ctx.User); err != nil {
		ctx.Flash.Error(err.Error())
		return
	}

	ctx.Flash.Success(ctx.Tr("settings.update_avatar_success"))
}

func SettingsEmails(ctx *middleware.Context) {
	ctx.Data["Title"] = ctx.Tr("settings")
	ctx.Data["PageIsSettingsEmails"] = true

	emails, err := models.GetEmailAddresses(ctx.User.Id)
	if err != nil {
		ctx.Handle(500, "GetEmailAddresses", err)
		return
	}
	ctx.Data["Emails"] = emails

	ctx.HTML(200, SETTINGS_EMAILS)
}

func SettingsEmailPost(ctx *middleware.Context, form auth.AddEmailForm) {
	ctx.Data["Title"] = ctx.Tr("settings")
	ctx.Data["PageIsSettingsEmails"] = true

	emails, err := models.GetEmailAddresses(ctx.User.Id)
	if err != nil {
		ctx.Handle(500, "GetEmailAddresses", err)
		return
	}
	ctx.Data["Emails"] = emails

	// Delete E-mail address.
	if ctx.Query("_method") == "DELETE" {
		id := ctx.QueryInt64("id")
		if id <= 0 {
			return
		}

		if err = models.DeleteEmailAddress(&models.EmailAddress{Id: id}); err != nil {
			ctx.Handle(500, "DeleteEmail", err)
		} else {
			log.Trace("Email address deleted: %s", ctx.User.Name)
			ctx.Redirect(setting.AppSubUrl + "/user/settings/email")
		}
		return
	}

	// Make emailaddress primary.
	if ctx.Query("_method") == "PRIMARY" {
		id := ctx.QueryInt64("id")
		if id <= 0 {
			return
		}

		if err = models.MakeEmailPrimary(&models.EmailAddress{Id: id}); err != nil {
			ctx.Handle(500, "MakeEmailPrimary", err)
		} else {
			log.Trace("Email made primary: %s", ctx.User.Name)
			ctx.Redirect(setting.AppSubUrl + "/user/settings/email")
		}
		return
	}

	// Add Email address.
	if ctx.HasError() {
		ctx.HTML(200, SETTINGS_EMAILS)
		return
	}

	cleanEmail := strings.Replace(form.Email, "\n", "", -1)
	e := &models.EmailAddress{
		Uid:         ctx.User.Id,
		Email:       cleanEmail,
		IsActivated: !setting.Service.RegisterEmailConfirm,
	}

	if err := models.AddEmailAddress(e); err != nil {
		if models.IsErrEmailAlreadyUsed(err) {
			ctx.RenderWithErr(ctx.Tr("form.email_been_used"), SETTINGS_EMAILS, &form)
			return
		}
		ctx.Handle(500, "AddEmailAddress", err)
		return
	} else {
		// Send confirmation e-mail
		if setting.Service.RegisterEmailConfirm {
			mailer.SendActivateEmail(ctx.Render, ctx.User, e)

			if err := ctx.Cache.Put("MailResendLimit_"+ctx.User.LowerName, ctx.User.LowerName, 180); err != nil {
				log.Error(4, "Set cache(MailResendLimit) fail: %v", err)
			}
			ctx.Flash.Info(ctx.Tr("settings.add_email_confirmation_sent", cleanEmail, setting.Service.ActiveCodeLives/60))
		} else {
			ctx.Flash.Success(ctx.Tr("settings.add_email_success"))
		}

		log.Trace("Email address added: %s", e.Email)
		ctx.Redirect(setting.AppSubUrl + "/user/settings/email")
		return
	}

	ctx.HTML(200, SETTINGS_EMAILS)
}

func SettingsPassword(ctx *middleware.Context) {
	ctx.Data["Title"] = ctx.Tr("settings")
	ctx.Data["PageIsSettingsPassword"] = true
	ctx.HTML(200, SETTINGS_PASSWORD)
}

func SettingsPasswordPost(ctx *middleware.Context, form auth.ChangePasswordForm) {
	ctx.Data["Title"] = ctx.Tr("settings")
	ctx.Data["PageIsSettingsPassword"] = true

	if ctx.HasError() {
		ctx.HTML(200, SETTINGS_PASSWORD)
		return
	}

	tmpUser := &models.User{
		Passwd: form.OldPassword,
		Salt:   ctx.User.Salt,
	}
	tmpUser.EncodePasswd()
	if ctx.User.Passwd != tmpUser.Passwd {
		ctx.Flash.Error(ctx.Tr("settings.password_incorrect"))
	} else if form.Password != form.Retype {
		ctx.Flash.Error(ctx.Tr("form.password_not_match"))
	} else {
		ctx.User.Passwd = form.Password
		ctx.User.Salt = models.GetUserSalt()
		ctx.User.EncodePasswd()
		if err := models.UpdateUser(ctx.User); err != nil {
			ctx.Handle(500, "UpdateUser", err)
			return
		}
		log.Trace("User password updated: %s", ctx.User.Name)
		ctx.Flash.Success(ctx.Tr("settings.change_password_success"))
	}

	ctx.Redirect(setting.AppSubUrl + "/user/settings/password")
}

func SettingsSSHKeys(ctx *middleware.Context) {
	ctx.Data["Title"] = ctx.Tr("settings")
	ctx.Data["PageIsSettingsSSHKeys"] = true

	keys, err := models.ListPublicKeys(ctx.User.Id)
	if err != nil {
		ctx.Handle(500, "ListPublicKeys", err)
		return
	}
	ctx.Data["Keys"] = keys

	ctx.HTML(200, SETTINGS_SSH_KEYS)
}

func SettingsSSHKeysPost(ctx *middleware.Context, form auth.AddSSHKeyForm) {
	ctx.Data["Title"] = ctx.Tr("settings")
	ctx.Data["PageIsSettingsSSHKeys"] = true

	keys, err := models.ListPublicKeys(ctx.User.Id)
	if err != nil {
		ctx.Handle(500, "ListPublicKeys", err)
		return
	}
	ctx.Data["Keys"] = keys

	if ctx.HasError() {
		ctx.HTML(200, SETTINGS_SSH_KEYS)
		return
	}

	content, err := models.CheckPublicKeyString(form.Content)
	if err != nil {
		if err == models.ErrKeyUnableVerify {
			ctx.Flash.Info(ctx.Tr("form.unable_verify_ssh_key"))
		} else {
			ctx.Flash.Error(ctx.Tr("form.invalid_ssh_key", err.Error()))
			ctx.Redirect(setting.AppSubUrl + "/user/settings/ssh")
			return
		}
	}

	if err = models.AddPublicKey(ctx.User.Id, form.Title, content); err != nil {
		ctx.Data["HasError"] = true
		switch {
		case models.IsErrKeyAlreadyExist(err):
			ctx.Data["Err_Content"] = true
			ctx.RenderWithErr(ctx.Tr("settings.ssh_key_been_used"), SETTINGS_SSH_KEYS, &form)
		case models.IsErrKeyNameAlreadyUsed(err):
			ctx.Data["Err_Title"] = true
			ctx.RenderWithErr(ctx.Tr("settings.ssh_key_name_used"), SETTINGS_SSH_KEYS, &form)
		default:
			ctx.Handle(500, "AddPublicKey", err)
		}
		return
	}

	ctx.Flash.Success(ctx.Tr("settings.add_key_success", form.Title))
	ctx.Redirect(setting.AppSubUrl + "/user/settings/ssh")
}

func DeleteSSHKey(ctx *middleware.Context) {
	if err := models.DeletePublicKey(ctx.QueryInt64("id")); err != nil {
		ctx.Flash.Error("DeletePublicKey: " + err.Error())
	} else {
		ctx.Flash.Success(ctx.Tr("settings.ssh_key_deletion_success"))
	}

	ctx.JSON(200, map[string]interface{}{
		"redirect": setting.AppSubUrl + "/user/settings/ssh",
	})
}

func SettingsSocial(ctx *middleware.Context) {
	ctx.Data["Title"] = ctx.Tr("settings")
	ctx.Data["PageIsSettingsSocial"] = true

	// Unbind social account.
	remove, _ := com.StrTo(ctx.Query("remove")).Int64()
	if remove > 0 {
		if err := models.DeleteOauth2ById(remove); err != nil {
			ctx.Handle(500, "DeleteOauth2ById", err)
			return
		}
		ctx.Flash.Success(ctx.Tr("settings.unbind_success"))
		ctx.Redirect(setting.AppSubUrl + "/user/settings/social")
		return
	}

	socials, err := models.GetOauthByUserId(ctx.User.Id)
	if err != nil {
		ctx.Handle(500, "GetOauthByUserId", err)
		return
	}
	ctx.Data["Socials"] = socials
	ctx.HTML(200, SETTINGS_SOCIAL)
}

func SettingsApplications(ctx *middleware.Context) {
	ctx.Data["Title"] = ctx.Tr("settings")
	ctx.Data["PageIsSettingsApplications"] = true

	tokens, err := models.ListAccessTokens(ctx.User.Id)
	if err != nil {
		ctx.Handle(500, "ListAccessTokens", err)
		return
	}
	ctx.Data["Tokens"] = tokens

	ctx.HTML(200, SETTINGS_APPLICATIONS)
}

func SettingsApplicationsPost(ctx *middleware.Context, form auth.NewAccessTokenForm) {
	ctx.Data["Title"] = ctx.Tr("settings")
	ctx.Data["PageIsSettingsApplications"] = true

	if ctx.HasError() {
		tokens, err := models.ListAccessTokens(ctx.User.Id)
		if err != nil {
			ctx.Handle(500, "ListAccessTokens", err)
			return
		}
		ctx.Data["Tokens"] = tokens
		ctx.HTML(200, SETTINGS_APPLICATIONS)
		return
	}

	t := &models.AccessToken{
		UID:  ctx.User.Id,
		Name: form.Name,
	}
	if err := models.NewAccessToken(t); err != nil {
		ctx.Handle(500, "NewAccessToken", err)
		return
	}

	ctx.Flash.Success(ctx.Tr("settings.generate_token_succees"))
	ctx.Flash.Info(t.Sha1)

	ctx.Redirect(setting.AppSubUrl + "/user/settings/applications")
}

func SettingsDeleteApplication(ctx *middleware.Context) {
	if err := models.DeleteAccessTokenByID(ctx.QueryInt64("id")); err != nil {
		ctx.Flash.Error("DeleteAccessTokenByID: " + err.Error())
	} else {
		ctx.Flash.Success(ctx.Tr("settings.delete_token_success"))
	}

	ctx.JSON(200, map[string]interface{}{
		"redirect": setting.AppSubUrl + "/user/settings/applications",
	})
}

func SettingsDelete(ctx *middleware.Context) {
	ctx.Data["Title"] = ctx.Tr("settings")
	ctx.Data["PageIsSettingsDelete"] = true

	if ctx.Req.Method == "POST" {
		if _, err := models.UserSignIn(ctx.User.Name, ctx.Query("password")); err != nil {
			if models.IsErrUserNotExist(err) {
				ctx.RenderWithErr(ctx.Tr("form.enterred_invalid_password"), SETTINGS_DELETE, nil)
			} else {
				ctx.Handle(500, "UserSignIn", err)
			}
			return
		}

		if err := models.DeleteUser(ctx.User); err != nil {
			switch {
			case models.IsErrUserOwnRepos(err):
				ctx.Flash.Error(ctx.Tr("form.still_own_repo"))
				ctx.Redirect(setting.AppSubUrl + "/user/settings/delete")
			case models.IsErrUserHasOrgs(err):
				ctx.Flash.Error(ctx.Tr("form.still_has_org"))
				ctx.Redirect(setting.AppSubUrl + "/user/settings/delete")
			default:
				ctx.Handle(500, "DeleteUser", err)
			}
		} else {
			log.Trace("Account deleted: %s", ctx.User.Name)
			ctx.Redirect(setting.AppSubUrl + "/")
		}
		return
	}

	ctx.HTML(200, SETTINGS_DELETE)
}
